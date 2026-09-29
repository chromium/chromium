# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit check for .pydeps files."""

from collections.abc import Mapping
import difflib
import os
import re
import shlex

_MISSING = object()


class PydepsChecker:
    def __init__(self, input_api, pydeps_files):
        self._file_cache = {}
        self._input_api = input_api
        self._pydeps_files = pydeps_files

    def _LoadFile(self, path):
        """Returns the contents of a .pydeps file."""
        data = self._file_cache.get(path)
        if data is None:
            with open(path, encoding='utf-8') as f:
                data = f.read()
            self._file_cache[path] = data
        return data

    def _ComputeNormalizedPydepsEntries(self, pydeps_path):
        """Returns an iterable of paths within the .pydep, relativized to //."""
        pydeps_data = self._LoadFile(pydeps_path)
        uses_gn_paths = '--gn-paths' in pydeps_data
        entries = (l for l in pydeps_data.splitlines() if not l.startswith('#'))
        if uses_gn_paths:
            # Paths look like: //foo/bar/baz
            return (e[2:] for e in entries)
        os_path = self._input_api.os_path
        pydeps_dir = os_path.dirname(pydeps_path)
        return (os_path.normpath(os_path.join(pydeps_dir, e)) for e in entries)

    def _CreateFilesToPydepsMap(self):
        """Returns a map of local_path -> list_of_pydeps."""
        ret = {}
        for pydep_local_path in self._pydeps_files:
            for path in self._ComputeNormalizedPydepsEntries(pydep_local_path):
                ret.setdefault(path, []).append(pydep_local_path)
        return ret

    @staticmethod
    def _GetGclientEval():
        try:
            import gclient_eval
        except ImportError:
            import find_depot_tools

            find_depot_tools.add_depot_tools_to_path()
            import gclient_eval
        return gclient_eval

    @staticmethod
    def _DiffDictKeys(old_dict, new_dict):
        if not isinstance(old_dict, Mapping) or not isinstance(
            new_dict, Mapping
        ):
            return set()
        return {
            key
            for key in old_dict.keys() | new_dict.keys()
            if old_dict.get(key, _MISSING) != new_dict.get(key, _MISSING)
        }

    @staticmethod
    def _FindChangedDeps(old_eval, new_eval):
        changed = PydepsChecker._DiffDictKeys(
            old_eval.get('deps') or {}, new_eval.get('deps') or {}
        )
        old_deps_os = old_eval.get('deps_os') or {}
        new_deps_os = new_eval.get('deps_os') or {}
        for os_name in set(old_deps_os) | set(new_deps_os):
            changed.update(
                PydepsChecker._DiffDictKeys(
                    old_deps_os.get(os_name) or {},
                    new_deps_os.get(os_name) or {},
                )
            )
        return changed

    @staticmethod
    def _ExtractConditions(eval_dict):
        if not isinstance(eval_dict, Mapping):
            return
        deps = eval_dict.get('deps') or {}
        if isinstance(deps, Mapping):
            for val in deps.values():
                if isinstance(val, Mapping):
                    cond = val.get('condition')
                    if isinstance(cond, str):
                        yield cond
        deps_os = eval_dict.get('deps_os') or {}
        if isinstance(deps_os, Mapping):
            for os_deps in deps_os.values():
                if isinstance(os_deps, Mapping):
                    for val in os_deps.values():
                        if isinstance(val, Mapping):
                            cond = val.get('condition')
                            if isinstance(cond, str):
                                yield cond

    @staticmethod
    def _IsVarReferencedInConditions(changed_vars, *eval_dicts):
        patterns = [re.compile(rf'\b{re.escape(v)}\b') for v in changed_vars]
        for eval_dict in eval_dicts:
            for cond in PydepsChecker._ExtractConditions(eval_dict):
                if any(p.search(cond) for p in patterns):
                    return True
        return False

    @staticmethod
    def _NormalizeChangedDeps(raw_deps):
        """Converts DEPS paths (e.g. 'src/third_party/foo') to repo-relative
        paths (e.g. 'third_party/foo'). Returns None if a dep affects repo
        root ('src').
        """
        changed_deps = set()
        for dep in raw_deps:
            norm = dep
            if norm == 'src' or norm.startswith('src/'):
                norm = norm[3:].lstrip('/')
            norm = norm.rstrip('/')
            if not norm:
                return None
            changed_deps.add(norm)
        return changed_deps

    @staticmethod
    def _GetChangedDeps(f):
        """Returns changed dep paths from DEPS, or None if repo root or config
        changed.
        """
        old_contents = '\n'.join(f.OldContents())
        new_contents = '\n'.join(f.NewContents())
        if not old_contents or not new_contents:
            return None

        gclient_eval = PydepsChecker._GetGclientEval()
        old_eval = gclient_eval.Exec(old_contents)
        new_eval = gclient_eval.Exec(new_contents)
        if not isinstance(old_eval, Mapping) or not isinstance(
            new_eval, Mapping
        ):
            return None

        changed_top_level = PydepsChecker._DiffDictKeys(old_eval, new_eval)
        if changed_top_level - {'deps', 'deps_os', 'vars'}:
            return None

        raw_deps = PydepsChecker._FindChangedDeps(old_eval, new_eval)
        changed_vars = PydepsChecker._DiffDictKeys(
            old_eval.get('vars') or {}, new_eval.get('vars') or {}
        )
        if changed_vars:
            if not raw_deps or PydepsChecker._IsVarReferencedInConditions(
                changed_vars, old_eval, new_eval
            ):
                return None

        return PydepsChecker._NormalizeChangedDeps(raw_deps)

    def ComputeAffectedPydeps(self):
        """Returns a set of .pydeps files that might need regenerating."""
        affected_pydeps = set()
        file_to_pydeps_map = None
        for f in self._input_api.AffectedFiles(include_deletes=True):
            local_path = f.LocalPath()
            # Changes to print_python_deps.py or pydeps_presubmit.py affect all
            # .pydeps.
            if local_path.endswith(
                ('print_python_deps.py', 'pydeps_presubmit.py')
            ):
                return set(self._pydeps_files)
            if local_path == 'DEPS':
                try:
                    changed_deps = self._GetChangedDeps(f)
                except Exception:
                    changed_deps = None
                if changed_deps is None:
                    return set(self._pydeps_files)
                if not changed_deps:
                    continue
                if file_to_pydeps_map is None:
                    file_to_pydeps_map = self._CreateFilesToPydepsMap()
                prefixes = tuple(f'{d}/' for d in changed_deps)
                for path, pydeps in file_to_pydeps_map.items():
                    norm_path = path.replace('\\', '/')
                    if (
                        norm_path.startswith(prefixes)
                        or norm_path in changed_deps
                    ):
                        affected_pydeps.update(pydeps)
            elif local_path.endswith('.pydeps'):
                if local_path in self._pydeps_files:
                    affected_pydeps.add(local_path)
            elif local_path.endswith('.py'):
                if file_to_pydeps_map is None:
                    file_to_pydeps_map = self._CreateFilesToPydepsMap()
                affected_pydeps.update(file_to_pydeps_map.get(local_path, ()))
        return affected_pydeps

    def CreateCheckCommand(self, output_api, pydeps_path):
        """Returns a Command that runs print_python_deps.py."""
        old_pydeps_data = self._LoadFile(pydeps_path).splitlines()
        if len(old_pydeps_data) >= 2:
            cmd = old_pydeps_data[1][1:].strip()
            if '--output' not in cmd:
                cmd += ' --output ' + pydeps_path
            old_contents = old_pydeps_data[2:]
        else:
            # A default cmd that should work in most cases (as long as pydeps
            # filename matches the script name) so that PRESUBMIT.py does not
            # crash if pydeps file is empty/new.
            cmd = 'build/print_python_deps.py {} --root={} --output={}'.format(
                pydeps_path[:-4], os.path.dirname(pydeps_path), pydeps_path
            )
            old_contents = []

        def parse_output(returncode, stdout, stderr):
            if returncode != 0:
                return [
                    output_api.PresubmitError(
                        f'Command failed: {cmd}\n{stdout}{stderr}'.rstrip()
                    )
                ]
            new_contents = stdout.splitlines()[2:]
            if old_contents != new_contents:
                diff = '\n'.join(
                    difflib.context_diff(old_contents, new_contents)
                )
                return [
                    output_api.PresubmitError(
                        'File is stale: {}\n'
                        'Diff (apply to fix):\n'
                        '{}\n'
                        'To regenerate, run:\n\n'
                        '    {}'.format(pydeps_path, diff, cmd)
                    )
                ]
            return []

        env = dict(os.environ)
        env['PYTHONDONTWRITEBYTECODE'] = '1'
        return self._input_api.Command(
            name='pydeps_presubmit: ' + pydeps_path,
            cmd=shlex.split(cmd + ' --output ""'),
            kwargs={'env': env},
            output_parser=parse_output,
        )


def _ParseGclientArgs():
    args = {}
    with open('build/config/gclient_args.gni', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            attribute, value = line.split('=')
            args[attribute.strip()] = value.strip()
    return args


def run_presubmit(
    input_api,
    output_api,
    all_pydeps_files,
    android_specific_pydeps_files,
    generic_pydeps_files,
    checker_for_tests=None,
):
    """Checks if a .pydeps file needs to be regenerated."""
    if not input_api.platform.startswith('linux'):
        return []

    results = []
    # First, check for new / deleted .pydeps.
    for f in input_api.AffectedFiles(include_deletes=True):
        # Check whether we are running the presubmit check for a file in src.
        if f.LocalPath().endswith('.pydeps'):
            # f.LocalPath is relative to repo (src, or internal repo).
            # os_path.exists is relative to src repo.
            # Therefore if os_path.exists is true, it means f.LocalPath is
            # relative to src and we can conclude that the pydeps is in src.
            exists = input_api.os_path.exists(f.LocalPath())
            if f.Action() == 'D' and f.LocalPath() in all_pydeps_files:
                results.append(
                    output_api.PresubmitError(
                        'Please update _ALL_PYDEPS_FILES within //PRESUBMIT.py '
                        'to remove %s' % f.LocalPath()
                    )
                )
            elif (
                f.Action() != 'D'
                and exists
                and f.LocalPath() not in all_pydeps_files
            ):
                results.append(
                    output_api.PresubmitError(
                        'Please update _ALL_PYDEPS_FILES within //PRESUBMIT.py '
                        'to include %s' % f.LocalPath()
                    )
                )

    if results:
        return results

    try:
        parsed_args = _ParseGclientArgs()
    except FileNotFoundError:
        message = (
            'build/config/gclient_args.gni not found. Please make sure your '
            'workspace has been initialized with gclient sync.'
        )
        # Users will always hit this when they run presubmits before cog
        # workspace initialization finishes. The check shouldn't fail in
        # this case. This is an unavoidable workaround that's needed for
        # good presubmit UX for cog.
        return [output_api.PresubmitPromptWarning(message)]

    is_android = parsed_args.get('checkout_android', 'false') == 'true'
    checker = checker_for_tests or PydepsChecker(input_api, all_pydeps_files)
    affected_pydeps = set(checker.ComputeAffectedPydeps())
    affected_android_pydeps = affected_pydeps.intersection(
        set(android_specific_pydeps_files)
    )
    if affected_android_pydeps and not is_android:
        results.append(
            output_api.PresubmitPromptOrNotify(
                """\
You have changed python files that may affect pydeps for android specific
scripts. However, the relevant presubmit check cannot be run because you are
not using an Android checkout. To validate that the .pydeps are correct, re-run
presubmit in an Android checkout, or use the android-internal-presubmit
optional trybot.

Possibly stale pydeps files:\n{}
""".format('\n'.join(affected_android_pydeps))
            )
        )

    all_pydeps = all_pydeps_files if is_android else generic_pydeps_files
    pydeps_to_check = affected_pydeps.intersection(all_pydeps)
    # Process these concurrently, as each one takes 1-2 seconds.
    for pydeps_path in sorted(pydeps_to_check):
        results.append(checker.CreateCheckCommand(output_api, pydeps_path))

    return input_api.RunTests(results)
