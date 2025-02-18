#!/usr/bin/env python3

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import os.path
import re
import shutil
import subprocess
import sys
import tempfile

# The path to `whole_archive`.
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

import whole_archive

# Prefix for all custom linker driver arguments.
LINKER_DRIVER_ARG_PREFIX = '-Wcrl,'
LINKER_DRIVER_COMPILER_ARG_PREFIX = '-Wcrl,driver,'

# The linker_driver.py is responsible for forwarding a linker invocation to
# the compiler driver, while processing special arguments itself.
#
# Usage: linker_driver.py -Wcrl,driver,clang++ main.o -L. -llib -o prog \
#            -Wcrl,dsym,out
#
# On Mac, the logical step of linking is handled by three discrete tools to
# perform the image link, debug info link, and strip. The linker_driver.py
# combines these three steps into a single tool.
#
# The compiler driver invocation for the linker is specified by the following
# required argument.
#
# -Wcrl,driver,<path_to_compiler_driver>
#    Specifies the path to the compiler driver.
#
# After running the compiler driver, the script performs additional actions,
# based on these arguments:
#
# -Wcrl,installnametoolpath,<install_name_tool_path>
#    Sets the path to the `install_name_tool` to run with
#    -Wcrl,installnametool, in which case `xcrun` is not used to invoke it.
#
# -Wcrl,installnametool,<arguments,...>
#    After invoking the linker, this will run install_name_tool on the linker's
#    output. |arguments| are comma-separated arguments to be passed to the
#    install_name_tool command.
#
# -Wcrl,dsym,<dsym_path_prefix>
#    After invoking the linker, this will run `dsymutil` on the linker's
#    output, producing a dSYM bundle, stored at dsym_path_prefix. As an
#    example, if the linker driver were invoked with:
#        "... -o out/gn/obj/foo/libbar.dylib ... -Wcrl,dsym,out/gn ..."
#    The resulting dSYM would be out/gn/libbar.dylib.dSYM/.
#
# -Wcrl,dsymutilpath,<dsymutil_path>
#    Sets the path to the dsymutil to run with -Wcrl,dsym, in which case
#    `xcrun` is not used to invoke it.
#
# -Wcrl,unstripped,<unstripped_path_prefix>
#    After invoking the linker, and before strip, this will save a copy of
#    the unstripped linker output in the directory unstripped_path_prefix.
#
# -Wcrl,strip,<strip_arguments>
#    After invoking the linker, and optionally dsymutil, this will run
#    the strip command on the linker's output. strip_arguments are
#    comma-separated arguments to be passed to the strip command.
#
# -Wcrl,strippath,<strip_path>
#    Sets the path to the strip to run with -Wcrl,strip, in which case
#    `xcrun` is not used to invoke it.
# -Wcrl,object_path_lto
#    Creates temporary directory for LTO object files.
#
# -Wcrl,otoolpath,<otool path>
#    Sets the path to the otool for solink process.
# -Wcrl,nmpath,<nm path>
#    Sets the path to the nm for solink process.
#
# -Wcrl.tocname,<tocname>
#    Output TOC for solink.
#    It would be processed both before the linker (to check reexport
#    in old module) and after the linker (to produce TOC if needed).

class LinkerDriver(object):
    def __init__(self, args):
        """Creates a new linker driver.

        Args:
            args: list of string, Arguments to the script.
        """
        self._args = args

        # List of linker driver pre-actions that need to run before the link.
        # **The sort order of this list affects the order in which
        # the actions are invoked.**
        # The first item in the tuple is the argument's -Wcrl,<sub_argument>
        # and the second is the function to invoke.
        self._pre_actions = [
            ('object_path_lto', self.prepare_object_path_lto),
            ('installnametoolpath,', self.set_install_name_tool_path),
            ('dsymutilpath,', self.set_dsymutil_path),
            ('strippath,', self.set_strip_path),
            ('otoolpath,', self.set_otool_path),
            ('nmpath,', self.set_nm_path),
            ('tocname,', self.check_reexport_in_old_module),
        ]

        # List of linker driver actions. **The sort order of this list affects
        # the order in which the actions are invoked.**
        # The first item in the tuple is the argument's -Wcrl,<sub_argument>
        # and the second is the function to invoke.
        self._actions = [
            ('installnametool,', self.run_install_name_tool),
            ('dsym,', self.run_dsymutil),
            ('unstripped,', self.run_save_unstripped),
            ('strip,', self.run_strip),
            ('tocname,', self.output_toc),
        ]

        # Linker driver actions can modify the these values.
        self._driver_path = None  # Must be specified on the command line.
        self._otool_cmd = ['xcrun', 'otool']
        self._nm_cmd = ['xcrun', 'nm']
        self._install_name_tool_cmd = ['xcrun', 'install_name_tool']
        self._dsymutil_cmd = ['xcrun', 'dsymutil']
        self._strip_cmd = ['xcrun', 'strip']

        # The linker output file, lazily computed in self._get_linker_output().
        self._linker_output = None

        # may not need to reexport unless LC_REEXPORT_DYLIB is used.
        self._reexport_in_old_module = False


    def run(self):
        """Runs the linker driver, separating out the main compiler driver's
        arguments from the ones handled by this class. It then invokes the
        required tools, starting with the compiler driver to produce the linker
        output.
        """
        # Collect arguments to the linker driver (this script) and remove them
        # from the arguments being passed to the compiler driver.
        self._linker_driver_actions = {}
        self._linker_driver_pre_actions = {}
        self._compiler_driver_args = []
        for index, arg in enumerate(self._args[1:]):
            if arg.startswith(LINKER_DRIVER_COMPILER_ARG_PREFIX):
                assert not self._driver_path
                self._driver_path = arg[len(LINKER_DRIVER_COMPILER_ARG_PREFIX
                                            ):]
            elif arg.startswith(LINKER_DRIVER_ARG_PREFIX):
                # Convert driver actions into a map of name => lambda to invoke.
                self._process_driver_arg(arg)
            else:
                # TODO(crbug.com/40268754): On Apple, the linker command line
                # produced by rustc for LTO includes these arguments, but the
                # Apple linker doesn't accept them.
                # Upstream bug: https://github.com/rust-lang/rust/issues/60059
                BAD_RUSTC_ARGS = '-Wl,-plugin-opt=O[0-9],-plugin-opt=mcpu=.*'
                if not re.match(BAD_RUSTC_ARGS, arg):
                    self._compiler_driver_args.append(arg)

        if not self._driver_path:
            raise RuntimeError(
                "Usage: linker_driver.py -Wcrl,driver,<compiler-driver> "
                "[linker-args]...")

        if self._get_linker_output() is None:
            raise ValueError(
                'Could not find path to linker output (-o or --output)')

        # We want to link rlibs as --whole-archive if they are part of a unit
        # test target. This is determined by switch
        # `-LinkWrapper,add-whole-archive`.
        self._compiler_driver_args = whole_archive.wrap_with_whole_archive(
            self._compiler_driver_args, is_apple=True)

        linker_driver_outputs = [self._get_linker_output()]

        try:
            # Zero the mtime in OSO fields for deterministic builds.
            # https://crbug.com/330262.
            env = os.environ.copy()
            env['ZERO_AR_DATE'] = '1'

            # Run the driver pre-actions, in the order specified by the
            # actions list.
            for action in self._pre_actions:
                name = action[0]
                if name in self._linker_driver_pre_actions:
                    self._linker_driver_pre_actions[name]()

            # Run the linker by invoking the compiler driver.
            subprocess.check_call([self._driver_path] +
                                  self._compiler_driver_args,
                                  env=env)

            # Run the linker driver actions, in the order specified by the
            # actions list.
            for action in self._actions:
                name = action[0]
                if name in self._linker_driver_actions:
                    linker_driver_outputs += self._linker_driver_actions[name](
                    )
        except:
            # If a linker driver action failed, remove all the outputs to make
            # the build step atomic.
            map(_remove_path, linker_driver_outputs)

            # Re-report the original failure.
            raise

    def _get_linker_output(self):
        """Returns the value of the output argument to the linker."""
        if not self._linker_output:
            for index, arg in enumerate(self._args):
                if arg in ('-o', '-output', '--output'):
                    self._linker_output = self._args[index + 1]
                    break
        return self._linker_output

    def _process_driver_arg(self, arg):
        """Processes a linker driver argument and returns a tuple containing the
        name and unary lambda to invoke for that linker driver action.

        Args:
            arg: string, The linker driver argument.

        Returns:
            A 2-tuple:
                0: The driver action name, as in |self._actions|.
                1: A lambda that calls the linker driver action with its direct
                   argument and returns a list of outputs from the action.
        """
        if not arg.startswith(LINKER_DRIVER_ARG_PREFIX):
            raise ValueError('%s is not a linker driver argument' % (arg, ))

        sub_arg = arg[len(LINKER_DRIVER_ARG_PREFIX):]

        found = False
        for driver_action in self._pre_actions:
            (pre_name, pre_action) = driver_action
            if sub_arg.startswith(pre_name):
                assert pre_name not in self._linker_driver_pre_actions, \
                    f"Name '{pre_name}' found in linker driver pre actions"
                self._linker_driver_pre_actions[pre_name] = \
                    lambda: pre_action(sub_arg[len(pre_name):])
                # same sub_arg may be used in actions.
                found = True
                break

        for driver_action in self._actions:
            (name, action) = driver_action
            if sub_arg.startswith(name):
                assert name not in self._linker_driver_actions, \
                    f"Name '{name}' found in linker driver actions"
                self._linker_driver_actions[name] = \
                        lambda: action(sub_arg[len(name):])
                return

        if not found:
            raise ValueError('Unknown linker driver argument: %s' % (arg, ))

    def prepare_object_path_lto(self, arg):
        """Linker driver pre-action for -Wcrl,object_path_lto.

        Prepare object_path_lto path in temp directory.
        """
        # TODO(lgrey): Remove if/when we start running `dsymutil`
        # through the clang driver. See https://crbug.com/1324104
        # The temporary directory for intermediate LTO object files. If it
        # exists, it will clean itself up on script exit.
        object_path_lto = tempfile.TemporaryDirectory(dir=os.getcwd())
        self._compiler_driver_args.append('-Wl,-object_path_lto,{}'.format(
            os.path.relpath(object_path_lto.name)))

    def check_reexport_in_old_module(self, tocname):
        """Linker driver pre-action for -Wcrl,tocname,<path>.

        Check whether it contains LC_REEXPORT_DYLIB in old module, so that
        needs to ouptupt TOC file for solink even if the same TOC.

        Returns:
           True if old module have LC_REEXPORT_DYLIB
        """
        if not os.path.exists(tocname):
            return
        dylib = self._get_linker_output()
        if not os.path.exists(dylib):
            return
        p = subprocess.run(self._otool_cmd + ['-l', dylib],
                           capture_output=True)
        if p.returncode != 0:
            return
        if re.match(rb'\s+cmd LC_REEXPORT_DYLIB$', p.stdout, re.MULTILINE):
            self._reexport_in_old_module = True

    def set_install_name_tool_path(self, install_name_tool_path):
        """Linker driver pre-action for -Wcrl,installnametoolpath,<path>.

        Sets the invocation command for install_name_tool, which allows the
        caller to specify an alternate path. This action is always
        processed before the run_install_name_tool action.

        Args:
            install_name_tool_path: string, The path to the install_name_tool
                binary to run
        """
        self._install_name_tool_cmd = [install_name_tool_path]

    def run_install_name_tool(self, args_string):
        """Linker driver action for -Wcrl,installnametool,<args>. Invokes
        install_name_tool on the linker's output.

        Args:
            args_string: string, Comma-separated arguments for
                `install_name_tool`.

        Returns:
            No output - this step is run purely for its side-effect.
        """
        command = list(self._install_name_tool_cmd)
        command.extend(args_string.split(','))
        command.append(self._get_linker_output())
        subprocess.check_call(command)
        return []

    def run_dsymutil(self, dsym_path_prefix):
        """Linker driver action for -Wcrl,dsym,<dsym-path-prefix>. Invokes
        dsymutil on the linker's output and produces a dsym file at |dsym_file|
        path.

        Args:
            dsym_path_prefix: string, The path at which the dsymutil output
                should be located.

        Returns:
            list of string, Build step outputs.
        """
        if not len(dsym_path_prefix):
            raise ValueError('Unspecified dSYM output file')

        linker_output = self._get_linker_output()
        base = os.path.basename(linker_output)
        dsym_out = os.path.join(dsym_path_prefix, base + '.dSYM')

        # Remove old dSYMs before invoking dsymutil.
        _remove_path(dsym_out)

        tools_paths = _find_tools_paths(self._args)
        if os.environ.get('PATH'):
            tools_paths.append(os.environ['PATH'])
        dsymutil_env = os.environ.copy()
        dsymutil_env['PATH'] = ':'.join(tools_paths)
        subprocess.check_call(self._dsymutil_cmd +
                              ['-o', dsym_out, linker_output],
                              env=dsymutil_env)
        return [dsym_out]

    def set_dsymutil_path(self, dsymutil_path):
        """Linker driver pre-action for -Wcrl,dsymutilpath,<dsymutil_path>.

        Sets the invocation command for dsymutil, which allows the caller to
        specify an alternate dsymutil. This action is always processed before
        the RunDsymUtil action.

        Args:
            dsymutil_path: string, The path to the dsymutil binary to run
        """
        self._dsymutil_cmd = [dsymutil_path]

    def run_save_unstripped(self, unstripped_path_prefix):
        """Linker driver action for -Wcrl,unstripped,<unstripped_path_prefix>.
        Copies the linker output to |unstripped_path_prefix| before stripping.

        Args:
            unstripped_path_prefix: string, The path at which the unstripped
                output should be located.

        Returns:
            list of string, Build step outputs.
        """
        if not len(unstripped_path_prefix):
            raise ValueError('Unspecified unstripped output file')

        base = os.path.basename(self._get_linker_output())
        unstripped_out = os.path.join(unstripped_path_prefix,
                                      base + '.unstripped')

        shutil.copyfile(self._get_linker_output(), unstripped_out)
        return [unstripped_out]

    def run_strip(self, strip_args_string):
        """Linker driver action for -Wcrl,strip,<strip_arguments>.

        Args:
            strip_args_string: string, Comma-separated arguments for `strip`.
        """
        strip_command = list(self._strip_cmd)
        if len(strip_args_string) > 0:
            strip_command += strip_args_string.split(',')
        strip_command.append(self._get_linker_output())
        subprocess.check_call(strip_command)
        return []

    def set_strip_path(self, strip_path):
        """Linker driver pre-action for -Wcrl,strippath,<strip_path>.

        Sets the invocation command for strip, which allows the caller to
        specify an alternate strip. This action is always processed before the
        RunStrip action.

        Args:
            strip_path: string, The path to the strip binary to run
        """
        self._strip_cmd = [strip_path]

    def set_otool_path(self, otool_path):
        """Linker driver pre-action for -Wcrl,otoolpath,<otool_path>.

        Sets the invocation command for otool.

        Args:
           otool_path: string. The path to the otool binary to run

        """
        self._otool_cmd = [otool_path]

    def set_nm_path(self, nm_path):
        """Linker driver pre-action for -Wcrl,nmpath,<nm_path>.

        Sets the invocation command for nm.

        Args:
           nm_path: string. The path to the nm binary to run

        Returns:
           No output - this step is run purely for its side-effect.
        """
        self._nm_cmd = [nm_path]

    def output_toc(self, tocname):
        """Linker driver action for -Wcrl,tocname,<path>.

        Produce *.TOC from linker output.

        TODO(ukai): recursively collect symbols from all 'LC_REEXPORT_DYLIB'-
        exported modules and present them all in the TOC, and
        drop self._reexport_in_old_module.

        Args:
           tocname: string, The path to *.TOC file.
        Returns:
           list of string, TOC file as output.
        """
        new_toc = self._extract_toc()
        old_toc = None
        if not self._reexport_in_old_module:
            try:
                with open(tocname, 'rb') as f:
                    old_toc = f.read()
            except OSError:
                pass

        if self._reexport_in_old_module or new_toc != old_toc:
            # TODO: use delete_on_close in python 3.12 or later.
            with tempfile.NamedTemporaryFile(prefix=tocname + '.',
                                             dir='.',
                                             delete=False) as f:
                f.write(new_toc)
                f.close()
                os.rename(f.name, tocname)
        return [tocname]

    def _extract_toc(self):
        """Extract TOC from linker output.

        Returns:
           output contents in bytes.
        """
        toc = b''
        dylib = self._get_linker_output()
        out = subprocess.check_output(self._otool_cmd + ['-l', dylib])
        lines = out.split(b'\n')
        found_id = False
        for i, line in enumerate(lines):
            # Too many LC_ID_DYLIBs? We didn’t understand something about
            # the otool output. Raise an exception and die, rather than
            # proceeding.

            # Not any LC_ID_DYLIBs? Probably not an MH_DYLIB. Probably fine, we
            # can proceed with ID-less TOC generation.
            if line == b'      cmd LC_ID_DYLIB':
                if found_id:
                    raise ValueError('Too many LC_ID_DYLIBs in %s' % dylib)
                toc += line + b'\n'
                for j in range(5):
                    toc += lines[i + 1 + j] + b'\n'
                found_id = True

        # -U ignores undefined symbols
        # -g display only global (external) symbols
        # -p unsorted https://crrev.com/c/2173969
        out = subprocess.check_output(self._nm_cmd + ['-Ugp', dylib])
        lines = out.split(b'\n')
        for line in lines:
            fields = line.split(b' ', 2)
            if len(fields) < 3:
                continue
            # fields = (value, type, name)
            # emit [type, name]
            toc += b' '.join(fields[1:3]) + b'\n'
        return toc


def _find_tools_paths(full_args):
    """Finds all paths where the script should look for additional tools."""
    paths = []
    for idx, arg in enumerate(full_args):
        if arg in ['-B', '--prefix']:
            paths.append(full_args[idx + 1])
        elif arg.startswith('-B'):
            paths.append(arg[2:])
        elif arg.startswith('--prefix='):
            paths.append(arg[9:])
    return paths


def _remove_path(path):
    """Removes the file or directory at |path| if it exists."""
    if os.path.exists(path):
        if os.path.isdir(path):
            shutil.rmtree(path)
        else:
            os.unlink(path)


if __name__ == '__main__':
    LinkerDriver(sys.argv).run()
    sys.exit(0)
