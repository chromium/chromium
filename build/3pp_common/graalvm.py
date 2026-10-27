# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers for building GraalVM native-image binaries in 3pp packages."""

import json
import logging
import os
import pathlib
import shutil
import tempfile
import urllib.request
import zipfile

import common

# Oracle GraalVM (rather than GraalVM Community Edition) is used because it
# supports profile-guided optimizations (PGO), which significantly improve the
# runtime performance of the generated binaries. It is distributed under the
# GraalVM Free Terms and Conditions (GFTC) license.
#
# Uses the latest release of this major version. Packages that use this module
# should pass version_strings_func=graalvm.version_strings to common.main() so
# that they are rebuilt when a new release comes out.
# LINT.IfChange(jdk_major_version)
_GRAALVM_MAJOR_VERSION = 25
# LINT.ThenChange(//third_party/jdk/3pp/3pp.py:jdk_major_version)
_GRAALVM_URL = (
    'https://download.oracle.com/graalvm/{v}/latest/'
    'graalvm-jdk-{v}_linux-x64_bin.tar.gz'
).format(v=_GRAALVM_MAJOR_VERSION)

# Without this wrapper, JAVA_HOME is not respected. The wrapper could be
# omitted if -Djava.home= is always set via command-line.
# Defaults to "." when JAVA_HOME is unset to satisfy non-null java.home checks
# (e.g., javac Locations.<clinit> in google-java-format) when no JDK files are
# read from disk.
_WRAPPER_TEMPLATE = """\
public class GraalVmWrapper {{
  public static void main(String[] args) throws Exception {{
    if (System.getProperty("java.home") == null) {{
      System.setProperty("java.home", {java_home_expr});
    }}
    {main_class}.main(args);
  }}
}}
"""

# Resolves a path relative to the directory containing the executable.
_EXE_RELATIVE_JAVA_HOME_TEMPLATE = (
    'java.nio.file.Paths.get("/proc/self/exe").toRealPath().getParent()'
    '.resolve("{path}").normalize().toString()'
)


def version_strings():
    """Returns strings that identify the GraalVM release that will be used.

    For use as common.main()'s version_strings_func.
    """
    # Oracle does not publish version numbers for "latest", but it does publish
    # the checksum of the archive.
    with urllib.request.urlopen(_GRAALVM_URL + '.sha256') as r:
        return [_GRAALVM_URL, r.read().decode('utf-8').split()[0]]


def _find_native_image(root):
    if os.path.isdir(root):
        for child in os.listdir(root):
            candidate = os.path.join(root, child)
            if os.path.exists(os.path.join(candidate, 'bin', 'native-image')):
                return candidate
    return None


def ensure_graalvm():
    """Downloads and extracts GraalVM if needed, returning its root path."""
    # This hook allows providing a local graalvm for local testing.
    env_home = os.environ.get('GRAALVM_HOME')
    if env_home and os.path.exists(
        os.path.join(env_home, 'bin', 'native-image')
    ):
        logging.info('Using GRAALVM_HOME=%s', env_home)
        return env_home

    extract_root = os.path.abspath('.graalvm')
    found = _find_native_image(extract_root)
    if found:
        return found

    filename = _GRAALVM_URL.rsplit('/', 1)[-1]
    cache_tar = os.path.join(tempfile.gettempdir(), filename)
    if not os.path.exists(cache_tar):
        tmp_tar = f'{cache_tar}.tmp.{os.getpid()}'
        common.download_file(_GRAALVM_URL, tmp_tar)
        os.replace(tmp_tar, cache_tar)

    os.makedirs(extract_root, exist_ok=True)
    common.extract_tar(cache_tar, extract_root)
    found = _find_native_image(extract_root)
    if found:
        return found

    raise RuntimeError(
        f'Failed to locate bin/native-image under {extract_root}'
    )


def _parse_manifest(jar_path):
    """Extracts (Main-Class, Add-Exports list) from a jar's MANIFEST.MF."""
    main_class = None
    add_exports = []
    with zipfile.ZipFile(jar_path) as z:
        if 'META-INF/MANIFEST.MF' not in z.namelist():
            return main_class, add_exports
        raw = z.read('META-INF/MANIFEST.MF').decode('utf-8', errors='replace')
    # Unwrap RFC 822 continuation lines (newline followed by single space).
    unwrapped = raw.replace('\r\n ', '').replace('\n ', '')
    for line in unwrapped.splitlines():
        if ':' not in line:
            continue
        key, val = line.split(':', 1)
        key = key.strip()
        val = val.strip()
        if key == 'Main-Class':
            main_class = val
        elif key == 'Add-Exports':
            add_exports.extend(val.split())
    return main_class, add_exports


def _merge_reachability_metadata(config_dir, extra_metadata):
    metadata_path = os.path.join(config_dir, 'reachability-metadata.json')
    if os.path.exists(metadata_path):
        with open(metadata_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
    else:
        data = {}

    for section in ('reflection', 'resources', 'jni', 'serialization'):
        items = extra_metadata.get(section)
        if items:
            data.setdefault(section, []).extend(items)

    with open(metadata_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2)


def _format_export(exp):
    return exp if '=' in exp else f'{exp}=ALL-UNNAMED'


def extract_jdk_sources(
    dest_dir, prefix='java.base/java/util/', recursive=False
):
    """Extracts .java files from GraalVM's src.zip (e.g. for PGO workloads).

    Args:
      dest_dir: Directory to extract to.
      prefix: Path prefix (or tuple of prefixes) within src.zip of files to
          extract.
      recursive: Whether to also extract files in subdirectories of prefix.
    Returns:
      Sorted list of absolute paths to the extracted files.
    """
    prefixes = (prefix,) if isinstance(prefix, str) else tuple(prefix)
    src_zip = os.path.join(ensure_graalvm(), 'lib', 'src.zip')
    ret = []
    with zipfile.ZipFile(src_zip) as z:
        for name in z.namelist():
            if not name.endswith('.java'):
                continue
            for p in prefixes:
                if name.startswith(p) and (
                    recursive or '/' not in name[len(p) :]
                ):
                    ret.append(os.path.abspath(z.extract(name, dest_dir)))
                    break
    return sorted(ret)


# jdk.internal.jrtfs.SystemImage computes the path to lib/modules (used by the
# jrt:/ filesystem) in its static initializer, from its code source. With
# Oracle GraalVM, this runs at image build time, and so bakes in the path of
# the GraalVM used to build the image. Patch it to use java.home instead, and
# initialize it at run time.
_FIND_HOME_SIGNATURE = '    private static String findHome() {\n'
_FIND_HOME_PATCH = (
    _FIND_HOME_SIGNATURE + '        if (Boolean.TRUE) {\n'
    '            return System.getProperty("java.home");\n'
    '        }\n'
)


def _jrt_file_system_args(graal_home, work_dir):
    """Returns native-image args needed for the jrt:/ filesystem to work."""
    src_dir = os.path.join(work_dir, 'java.base_src')
    system_image_java = os.path.join(
        src_dir, 'jdk', 'internal', 'jrtfs', 'SystemImage.java'
    )
    src_zip = os.path.join(graal_home, 'lib', 'src.zip')
    with zipfile.ZipFile(src_zip) as z:
        data = z.read('java.base/jdk/internal/jrtfs/SystemImage.java')
    data = data.decode('utf-8')
    if _FIND_HOME_SIGNATURE not in data:
        raise RuntimeError('Failed to patch SystemImage.java')
    os.makedirs(os.path.dirname(system_image_java))
    pathlib.Path(system_image_java).write_text(
        data.replace(_FIND_HOME_SIGNATURE, _FIND_HOME_PATCH, 1),
        encoding='utf-8',
    )
    classes_dir = os.path.join(work_dir, 'java.base_classes')
    common.run_cmd(
        [
            os.path.join(graal_home, 'bin', 'javac'),
            '--patch-module',
            f'java.base={src_dir}',
            '-d',
            classes_dir,
            system_image_java,
        ]
    )
    return [
        f'-J--patch-module=java.base={classes_dir}',
        '--initialize-at-run-time=jdk.internal.jrtfs.SystemImage',
    ]


def build_native_image(
    jar_path,
    output_path,
    *,
    main_class=None,
    extra_cp=None,
    add_exports=None,
    add_opens=None,
    tracing_args_list=None,
    extra_reachability_metadata=None,
    extra_native_image_args=None,
    pgo_args_list=None,
    java_home_relative_to_executable=None,
    check_workloads=False,
):
    """Builds a GraalVM native-image binary from a jar file.

    Args:
      jar_path: Path to the main .jar file. May be None when main_class is
          set (e.g. for main classes that are part of the JDK).
      output_path: Destination path for the compiled executable.
      main_class: Main class name. If None, read from MANIFEST.MF.
      extra_cp: Optional list of .jar paths to prepend to the classpath.
      add_exports: Optional list of module/package strings for --add-exports.
      add_opens: Optional list of module/package strings for --add-opens (for
          packages that are accessed reflectively at runtime).
      tracing_args_list: Optional list of argv lists to run under the GraalVM
          tracing agent (-agentlib:native-image-agent) before compiling.
      extra_reachability_metadata: Optional dict with 'reflection' and/or
          'resources' lists to merge into reachability-metadata.json.
      extra_native_image_args: Optional list of extra flags for native-image.
          If this contains -H:+AllowJRTFileSystem, the jrt:/ filesystem reads
          $java.home/lib/modules.
          Unless this contains a -march= flag, -march=compatibility is used.
      pgo_args_list: Optional list of argv lists to run an instrumented binary
          with in order to collect profiles for profile-guided optimizations.
          These should be representative of real workloads.
      java_home_relative_to_executable: When set, java.home defaults to this
          path relative to the directory containing the executable (rather
          than to $JAVA_HOME or "."), like the JDK's own launchers do.
      check_workloads: Whether to fail if tracing or PGO runs exit non-zero.
    """
    graal_home = ensure_graalvm()
    java_bin = os.path.join(graal_home, 'bin', 'java')
    javac_bin = os.path.join(graal_home, 'bin', 'javac')
    native_image_bin = os.path.join(graal_home, 'bin', 'native-image')

    output_path = os.path.abspath(output_path)

    manifest_exports = []
    if jar_path:
        jar_path = os.path.abspath(jar_path)
        manifest_main, manifest_exports = _parse_manifest(jar_path)
        if not main_class:
            main_class = manifest_main
    if not main_class:
        raise ValueError(f'No main_class specified or found in {jar_path}')

    all_exports = []
    for exp in manifest_exports + (add_exports or []):
        formatted = _format_export(exp)
        if formatted not in all_exports:
            all_exports.append(formatted)
    all_opens = []
    for x in add_opens or []:
        formatted = _format_export(x)
        if formatted not in all_opens:
            all_opens.append(formatted)

    cp_entries = [os.path.abspath(p) for p in (extra_cp or [])]
    if jar_path:
        cp_entries.append(jar_path)
    classpath = os.pathsep.join(cp_entries)

    with tempfile.TemporaryDirectory(
        prefix='graalvm_build_', dir='.'
    ) as work_dir:
        config_dir = os.path.abspath(os.path.join(work_dir, 'config'))
        os.makedirs(config_dir, exist_ok=True)

        # 1. Run the tracing agent if tracing commands are provided.
        for tracing_args in tracing_args_list or []:
            agent_flag = (
                f'-agentlib:native-image-agent=config-merge-dir={config_dir}'
            )
            agent_cmd = [java_bin, agent_flag]
            for exp in all_exports:
                agent_cmd.append(f'--add-exports={exp}')
            for x in all_opens:
                agent_cmd.append(f'--add-opens={x}')
            if classpath:
                agent_cmd.extend(['-cp', classpath])
            agent_cmd.append(main_class)
            agent_cmd.extend(tracing_args)
            # Tracing runs may intentionally exit non-zero (e.g. checkstyle).
            common.run_cmd(agent_cmd, check=check_workloads)

        if tracing_args_list and not os.listdir(config_dir):
            raise RuntimeError(
                f'Tracing agent failed to generate config in {config_dir}'
            )

        if extra_reachability_metadata:
            _merge_reachability_metadata(
                config_dir, extra_reachability_metadata
            )

        # 2. Compile GraalVmWrapper so java.home is initialized at startup.
        wrapper_dir = os.path.join(work_dir, 'wrapper')
        os.makedirs(wrapper_dir, exist_ok=True)
        wrapper_java = os.path.join(wrapper_dir, 'GraalVmWrapper.java')
        if java_home_relative_to_executable:
            java_home_expr = _EXE_RELATIVE_JAVA_HOME_TEMPLATE.format(
                path=java_home_relative_to_executable
            )
        else:
            java_home_expr = (
                'System.getenv("JAVA_HOME") != null'
                ' ? System.getenv("JAVA_HOME") : "."'
            )
        pathlib.Path(wrapper_java).write_text(
            _WRAPPER_TEMPLATE.format(
                main_class=main_class, java_home_expr=java_home_expr
            ),
            encoding='utf-8',
        )

        javac_cmd = [javac_bin]
        for exp in all_exports:
            javac_cmd.append(f'--add-exports={exp}')
        if classpath:
            javac_cmd.extend(['-cp', classpath])
        javac_cmd.extend(['-d', wrapper_dir, wrapper_java])
        common.run_cmd(javac_cmd)

        # 3. Build the native image into work_dir, then copy only the executable
        # to output_path so any unused auxiliary JDK .so files (libjsound.so)
        # do not pollute output_prefix.
        full_cp = os.pathsep.join([os.path.abspath(wrapper_dir)] + cp_entries)

        extra_native_image_args = list(extra_native_image_args or [])
        if '-H:+AllowJRTFileSystem' in extra_native_image_args:
            extra_native_image_args += _jrt_file_system_args(
                graal_home, work_dir
            )

        def run_native_image(output, extra_args):
            ni_cmd = [
                native_image_bin,
                '-cp',
                full_cp,
                f'-H:ConfigurationFileDirectories={config_dir}',
                '-o',
                output,
            ]
            for exp in all_exports:
                ni_cmd.append(f'-J--add-exports={exp}')
            # Also applies at runtime (e.g. for setAccessible()).
            for x in all_opens:
                ni_cmd.append(f'--add-opens={x}')
            # native-image defaults to -march=x86-64-v3 (AVX2), which fails to
            # start on older hosts. Some intrinsics still use AVX and AVX2 when
            # available via -H:RuntimeCheckedCPUFeatures (default: AVX,AVX2).
            if not any(
                a.startswith('-march=') for a in extra_native_image_args
            ):
                ni_cmd.append('-march=compatibility')
            ni_cmd.extend(extra_native_image_args)
            ni_cmd.extend(extra_args)
            ni_cmd.append('GraalVmWrapper')
            common.run_cmd(ni_cmd)

        final_args = []
        if pgo_args_list:
            # 3a. Build an instrumented binary and collect profiles with it.
            instrumented = os.path.abspath(
                os.path.join(work_dir, 'instrumented')
            )
            run_native_image(instrumented, ['--pgo-instrument'])
            profiles = []
            for i, pgo_args in enumerate(pgo_args_list):
                profile = os.path.abspath(
                    os.path.join(work_dir, f'profile{i}.iprof')
                )
                # Like tracing runs, training runs may exit non-zero.
                common.run_cmd(
                    [
                        instrumented,
                        f'-XX:ProfilesDumpFile={profile}',
                        f'-Djava.home={graal_home}',
                    ]
                    + pgo_args,
                    check=check_workloads,
                )
                if not os.path.exists(profile):
                    raise RuntimeError(
                        f'PGO training run did not write {profile}'
                    )
                profiles.append(profile)
            final_args = ['-O3', '--pgo=' + ','.join(profiles)]

        temp_output = os.path.abspath(
            os.path.join(work_dir, os.path.basename(output_path))
        )
        run_native_image(temp_output, final_args)
        # Ensure the path of this GraalVM's lib/modules has not been baked in.
        modules_path = os.path.join(graal_home, 'lib', 'modules')
        if (
            modules_path.encode('utf-8')
            in pathlib.Path(temp_output).read_bytes()
        ):
            raise RuntimeError(f'{output_path} contains {modules_path}')
        shutil.copy(temp_output, output_path)
