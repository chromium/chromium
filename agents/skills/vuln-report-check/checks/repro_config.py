# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The `repro.json` file tells Chrome's test system how to run the bug.

The file names the browser or shell, the build, the command-line flags, and
the attached file to open. Checks that the file is attached, that every key is
filled in, and that the values make sense and match the attached files. Also
checks an attached `vrp-flag.json`. When something is wrong, prints a
pre-filled template for the researcher to complete.
"""

from collections.abc import Iterable, Mapping, Sequence
import importlib
import re
import sys
from typing import Any

common = importlib.import_module(
    f'{__package__}.common' if __package__ else 'common'
)

ORDER = 5
CANONICAL_BINARIES = ('chrome', 'd8')
ENVIRONMENT_BINARIES = (('chrome-desktop', 'chrome'), ('d8', 'd8'))
MANUAL_BUILDS = ('msan', 'ubsan', 'hwasan', 'debug', 'official')
BUILDS = ('asan', 'release') + MANUAL_BUILDS
ARCHES = ('x64', 'arm64', 'x86', 'arm')
REQUIRED_PORT = 8000
CANONICAL_OS_PAIRS = (
    ('linux', 'Linux'),
    ('windows', 'Windows'),
    ('macos', 'macOS'),
    ('mac', 'macOS'),
    ('android', 'Android'),
    ('ios', 'iOS'),
    ('chromeos', 'ChromeOS'),
    ('fuchsia', 'Fuchsia'),
)
MANUAL_ONLY_OS = ('Android', 'iOS', 'ChromeOS', 'Fuchsia')
# Extra keys each manual environment needs, from references/repro_config.md.
ENVIRONMENT_REQUIRED_KEYS = (
    ('content_shell', ('commit', 'gn_args')),
    (
        'chrome-android',
        (
            'device',
            'os_version',
            'package',
            'apk',
            'command_line',
            'poc_delivery',
        ),
    ),
    (
        'android-webview',
        (
            'device',
            'os_version',
            'webview_provider',
            'host_app',
            'webview_settings',
        ),
    ),
    ('chrome-ios', ('device', 'os_version', 'chrome_specific_code')),
    (
        'chromeos',
        (
            'device',
            'os_version',
            'channel',
            'device_mode',
            'account',
            'entry_point',
        ),
    ),
    ('chrome-extension', ('manifest_version', 'install_method', 'permissions')),
    ('fuzzer', ('fuzz_target', 'testcase', 'commit')),
)
# `command_line: null` means the device runs Chrome without extra flags.
NULLABLE_ENVIRONMENT_KEYS = frozenset({'command_line'})
CHROME_VERSION_RE = re.compile(r'\d+\.\d+\.\d+\.\d+')
V8_VERSION_RE = re.compile(r'\d+\.\d+\.\d+(?:\.\d+)?')
PATCH_SUFFIXES = ('.diff', '.patch')
REPRO_PATCH_NAME = 'patch.diff'
# Mirrors the automated reproduction pipeline's word-bounded fix-patch filter
# so filenames such as `fixed.diff` classify identically during validation.
FIX_PATCH_RE = re.compile(
    r'(?:^|[^a-z])(?:bug|hot)?fix(?:es|up)?(?:[^a-z]|$)', re.IGNORECASE
)
SANITIZER_RE = re.compile(
    r'(?:AddressSanitizer|MemorySanitizer|UndefinedBehaviorSanitizer|'
    r'ThreadSanitizer|HWAddressSanitizer|LeakSanitizer|'
    r'==\d+==(?:ERROR|WARNING):)',
    re.IGNORECASE,
)
CRASH_HEADER_RE = re.compile(
    r'(?:==\d+==(?:ERROR|WARNING):|CHECK failed:|DCHECK failed:|'
    r'FATAL:|signal \d+ \(SIG\w+\)|Segmentation fault|SEGV)',
    re.IGNORECASE,
)
SYMBOLIZED_FRAME_RE = re.compile(
    r'#\d+\s+(?:pc\s+)?(?:0x)?[0-9a-fA-F]+\s+(?:in\s+)?(?!<unknown)[^\s(]+'
)
UNSYMBOLIZED_MODULE_RE = re.compile(
    r'\(<unknown module>\)|\bin\s+<unknown>', re.IGNORECASE
)
CRASH_LOG_SUFFIXES = ('.txt', '.log')
CRASH_LOG_NAME_HINTS = ('asan', 'tombstone', 'crash')
CANONICAL_CRASH_LOG_NAMES = frozenset(
    {'asan.txt', 'logcat.txt', 'tombstone.txt'}
)
WINDOWS_PATH_RE = re.compile(r'[A-Za-z]:\\')
NONE_LIKE_STRINGS = frozenset({'none', 'n/a', 'na', 'not applicable'})
PLACEHOLDER_RE = re.compile(
    r'\[(?:x\.x\.x\.x|goes here|steps go here|stable, beta, or dev|'
    r'tab, browser, etc\.|please indicate [^\]]{0,200}|'
    r'see link above:[^\]]{0,200})\]',
    re.IGNORECASE,
)
ENV_PRECONDITION_CONFLICT_RE = re.compile(
    r'(?:/etc/hosts\b|--host-resolver-rules\b|enterprise\s+policy|'
    r'managed\s+policy\s+json)',
    re.IGNORECASE,
)
USER_GESTURE_STEPS_RE = re.compile(
    r'\b(?:click|double-click|right-click|drag|drop|press\s+(?:the\s+)?key|'
    r'type\s+into|allow\s+button|permission\s+prompt)\b',
    re.IGNORECASE,
)
STEPS_HEADER_RE = re.compile(
    r'^[ \t]*steps(?:[ \t]+to[ \t]+reproduce)?[ \t]*:', re.IGNORECASE
)
VRP_FLAG_KINDS = ('read', 'write')
VRP_PROCESSES = ('browser', 'network', 'gpu', 'renderer')
VRP_OS_ALIASES = (
    ('win', 'Windows'),
    ('mac', 'macOS'),
    ('linux', 'Linux'),
    ('android', 'Android'),
    ('chromeos', 'ChromeOS'),
)
VRP_ASAN_POC = 'poc-asan.html'
_CONFIG = common.CONFIG_FILE_NAME


def _fail_both(check_id: str, message: str, fix: str) -> list[common.Check]:
    """Returns two failing checks: one for report quality, one for repro.

    The same problem hurts both the report and the automated reproduction,
    so it is recorded on both axes. The repro copy gets a `.repro` suffix.
    """
    return [
        common.Check(check_id, common.FAIL, message, fix, common.AXIS_QUALITY),
        common.Check(
            f'{check_id}.repro', common.FAIL, message, fix, common.AXIS_REPRO
        ),
    ]


def _pass_both(check_id: str, message: str) -> list[common.Check]:
    """Returns two passing checks: one for report quality, one for repro.

    Mirrors `_fail_both` so both axes always get a result.
    """
    return [
        common.Check(check_id, common.PASS, message, axis=common.AXIS_QUALITY),
        common.Check(
            f'{check_id}.repro', common.PASS, message, axis=common.AXIS_REPRO
        ),
    ]


def _manual(check_id: str, message: str) -> common.Check:
    """Returns a repro check saying a human must verify this report.

    The result is a PASS, not a FAIL: the report is fine, but the automated
    reproduction system cannot run this setup, so a person will.
    """
    return common.Check(
        f'{check_id}.repro',
        common.PASS,
        message,
        fix=common.MANUAL_SUGGESTION,
        axis=common.AXIS_REPRO,
        manual=True,
    )


def _quote(value: Any) -> str:
    """Returns a short text copy of a value for use in messages.

    Values come from the researcher's file, so they are cut to 80
    characters to keep messages readable.
    """
    return common.excerpt(str(value), 80)


def _is_crash_log_name(name: str) -> bool:
    """Returns whether a file name looks like a crash log.

    The name must end in .txt or .log and contain a hint such as "asan",
    "tombstone" or "crash".
    """
    lowered = name.lower()
    return lowered.endswith(CRASH_LOG_SUFFIXES) and any(
        hint in lowered for hint in CRASH_LOG_NAME_HINTS
    )


def _crash_log_candidates(names: Iterable[str]) -> list[str]:
    """Returns names that look like crash logs, standard names first.

    Researchers often attach variants (`asan-poison-history.txt`) next to the
    main trace. Sorting by name alone would suggest the variant, so the
    standard names (asan.txt, logcat.txt, tombstone.txt) come first.
    """
    candidates = [n for n in names if _is_crash_log_name(n)]
    # Sort key: standard names first, then shorter names, then by name.
    return sorted(
        candidates,
        key=lambda n: (n.lower() not in CANONICAL_CRASH_LOG_NAMES, len(n), n),
    )


def _has_env_value(raw: Mapping[str, Any], key: str) -> bool:
    """Returns whether an environment-specific key has a real value.

    Empty strings, empty lists and empty objects do not count. `null` only
    counts for keys where it has a meaning (see NULLABLE_ENVIRONMENT_KEYS).
    """
    if key not in raw:
        return False
    value = raw[key]
    if value is None:
        return key in NULLABLE_ENVIRONMENT_KEYS
    if isinstance(value, str):
        return bool(value.strip())
    return value not in ([], {})


def _check_environment(
    submission: common.Submission, config: common.ReproConfig
) -> list[common.Check]:
    """Checks the `environment` value and the extra keys it needs.

    Desktop environments run automatically. Other environments (Android,
    iOS, extensions, fuzzers, ...) go to a human, who needs extra details
    such as the device or commit to set up the test.
    """
    env = config.values['environment']
    # gtest is not an accepted environment. Name it so the researcher gets a
    # clear reason instead of "Unknown environment".
    if env == 'gtest':
        submission.set_field('environment', common.INVALID, None)
        return _fail_both(
            'config.environment',
            'Unit and browser tests (gtest) are not accepted.',
            'Attach a poc.html or poc.js that reproduces the bug in Chrome or'
            ' d8.',
        )
    if env not in common.ENVIRONMENTS:
        submission.set_field('environment', common.INVALID, None)
        return _fail_both(
            'config.environment',
            f'Unknown environment "{_quote(env)}" in {_CONFIG}.',
            f'Use one of: {", ".join(common.ENVIRONMENTS)}.',
        )
    submission.set_field('environment', common.VALUE, env)
    if env in common.DESKTOP_ENVIRONMENTS:
        return _pass_both(
            'config.environment',
            f'Environment "{env}" is in the automated reproduction matrix.',
        )
    missing = [
        key
        for key in dict(ENVIRONMENT_REQUIRED_KEYS)[env]
        if not _has_env_value(config.raw, key)
    ]
    # Installing an extension is itself a user gesture, so it must be stated.
    if env == 'chrome-extension' and config.values['user_interaction'] is None:
        missing.append('user_interaction')
    checks = [
        _manual(
            'config.environment',
            f'Environment "{env}" routes to manual verification.',
        )
    ]
    if missing:
        checks.append(
            common.Check(
                'config.environment',
                common.FAIL,
                f'Environment "{env}" needs these {_CONFIG} keys:'
                f' {", ".join(missing)}.',
                fix=f'Add non-empty values for: {", ".join(missing)}.',
                axis=common.AXIS_QUALITY,
            )
        )
    else:
        checks.append(
            common.Check(
                'config.environment',
                common.PASS,
                f'All keys present for environment "{env}".',
                axis=common.AXIS_QUALITY,
            )
        )
    return checks


def _check_binary(
    submission: common.Submission, values: Mapping[str, Any]
) -> list[common.Check]:
    """Checks that `binary` fits the environment and the PoC file type.

    Only "chrome" and "d8" (V8's command-line JavaScript shell) are allowed.
    A .js PoC needs d8 and an .html PoC needs chrome, or the test cannot run.
    """
    env = values['environment']
    binary = values['binary']
    poc = (values['poc'] or '').lower()
    env_binary = dict(ENVIRONMENT_BINARIES).get(env)
    # `null` is fine only for environments that do not run a fixed binary.
    if binary is None:
        if env_binary is None:
            submission.set_field('binary', common.NONE, None)
            return []
        submission.set_field('binary', common.INVALID, None)
        return _fail_both(
            'config.binary',
            f'"binary" is null, but environment "{env}" runs `{env_binary}`.',
            f'Set "binary": "{env_binary}".',
        )
    # Paths such as "out/asan/chrome" are rejected; only the bare name works.
    if binary not in CANONICAL_BINARIES:
        submission.set_field('binary', common.INVALID, None)
        expected = 'd8' if env == 'd8' or poc.endswith('.js') else 'chrome'
        return _fail_both(
            'config.binary',
            f'Disallowed or non-canonical binary "{_quote(binary)}".',
            f'Write "{expected}", not "{_quote(binary)}" (omit build'
            ' directories and paths).',
        )
    conflict = ''
    if env_binary is not None and binary != env_binary:
        conflict = f'Binary "{binary}" conflicts with environment "{env}".'
    elif poc.endswith(('.js', '.mjs')) and binary != 'd8':
        conflict = f'Binary "{binary}" cannot run the JavaScript PoC.'
    elif poc.endswith(('.html', '.htm')) and binary != 'chrome':
        conflict = f'Binary "{binary}" cannot open the HTML PoC.'
    if conflict:
        submission.set_field('binary', common.CONFLICT, binary)
        return _fail_both(
            'config.binary',
            conflict,
            'Use "d8" for .js PoCs and "chrome" for .html PoCs, matching'
            ' "environment".',
        )
    submission.set_field('binary', common.VALUE, binary)
    return [
        common.Check(
            'config.binary',
            common.PASS,
            f'Binary "{binary}" is valid.',
            axis=common.AXIS_QUALITY,
        )
    ]


def _check_build(
    submission: common.Submission, values: Mapping[str, Any], crash_text: str
) -> list[common.Check]:
    """Checks the `build` value against the list and the crash log.

    A release or official build cannot produce a sanitizer trace (the
    detailed memory-error report from tools like ASan), so that pair is a
    conflict. Builds other than asan and release go to a human.
    """
    raw_build = values['build']
    build = raw_build.strip().lower()
    if build not in BUILDS:
        submission.set_field('build', common.INVALID, None)
        return _fail_both(
            'config.build',
            f'Invalid or unsupported build "{_quote(raw_build)}".',
            f'Use one of: {", ".join(BUILDS)}.',
        )
    if build in ('release', 'official') and SANITIZER_RE.search(crash_text):
        submission.set_field('build', common.CONFLICT, build)
        return _fail_both(
            'config.build',
            f'Build "{build}" conflicts with the sanitizer trace in'
            ' "crash_log".',
            'Set "build": "asan" (or the matching sanitizer), or attach the'
            ' trace from the build you named.',
        )
    submission.set_field('build', common.VALUE, build)
    checks = [
        common.Check(
            'config.build',
            common.PASS,
            f'Build "{build}" is valid.',
            axis=common.AXIS_QUALITY,
        )
    ]
    if build in MANUAL_BUILDS:
        checks.append(
            _manual(
                'config.build',
                f'Build "{build}" routes to manual verification.',
            )
        )
    return checks


def _check_flags(
    submission: common.Submission,
    values: Mapping[str, Any],
    poc_text: str | None,
) -> list[common.Check]:
    """Checks that `flags` lists only command-line switches.

    Whether a d8 PoC that calls V8 internal functions (`%Foo()`) also lists
    `--allow-natives-syntax` is left to the agent, so no JavaScript has to be
    parsed here.
    """
    flags = values['flags']
    bad = [flag for flag in flags if len(flag) < 2 or not flag.startswith('-')]
    if bad:
        submission.set_field('flags', common.INVALID, flags)
        return _fail_both(
            'config.flags',
            f'Entries in "flags" must be command-line switches: {_quote(bad)}.',
            'List only switches such as "--enable-features=Foo"; name the PoC'
            ' in "poc", not in "flags".',
        )
    submission.set_field('flags', common.VALUE, flags)
    return [
        common.Check(
            'config.flags',
            common.PASS,
            f'{len(flags)} flag(s) listed.',
            axis=common.AXIS_QUALITY,
        )
    ]


def _check_entry(
    submission: common.Submission,
    values: Mapping[str, Any],
    names: frozenset[str],
) -> list[common.Check]:
    """Checks that `poc` and `server.file` name attached files.

    The test system opens exactly these files, so a missing, misnamed or
    metadata file means nothing runs. Automatic environments need at least
    one of them; manual ones may have neither.
    """
    env = values['environment']
    server = values['server']
    entries = (
        ('poc', values['poc']),
        ('server', server['file'] if server is not None else None),
    )
    problems: list[str] = []
    for key, name in entries:
        if name is None:
            submission.set_field(key, common.NONE, None)
        elif common.is_metadata_file(name):
            submission.set_field(key, common.INVALID, None)
            problems.append(f'"{key}" names a metadata file, not a PoC')
        elif key == 'server' and not name.endswith('.py'):
            submission.set_field(key, common.INVALID, None)
            problems.append(
                f'"server.file" must be a Python script, got {name!r}'
            )
        elif name not in names:
            submission.set_field(key, common.CONFLICT, name)
            problems.append(
                f'"{key}" names {_quote(name)!r}, which is not attached'
            )
        else:
            submission.set_field(key, common.VALUE, name)
    if (
        values['poc'] is None
        and server is None
        and env not in common.MANUAL_ENVIRONMENTS
    ):
        problems.append('neither "poc" nor "server" names a file to run')
    if problems:
        return _fail_both(
            'config.entry',
            f'{_CONFIG}: ' + '; '.join(problems) + '.',
            'Set "poc" to an attached poc.html or index.html (chrome) or poc.js'
            ' (d8), or "server" to {"file": "server.py", "port": 8000}.',
        )
    return _pass_both('config.entry', 'PoC entry files are attached.')


def _check_patches(
    submission: common.Submission,
    values: Mapping[str, Any],
    names: frozenset[str],
) -> list[common.Check]:
    """Matches `patch` and `proposed_fix` to the attached diff files.

    Two kinds of diff exist. A reproduction patch (`patch.diff`) is applied
    to build the bug. A proposed fix (a name with "fix") is not applied.
    Diffs are sorted into the two kinds by name, the same way Chrome's
    automated reproduction system does, so both agree.
    """
    patch = values['patch']
    proposed = values['proposed_fix']
    diffs = sorted(n for n in names if n.lower().endswith(PATCH_SUFFIXES))
    # Any diff that is not a fix is treated as a reproduction patch.
    fix_diffs = [n for n in diffs if FIX_PATCH_RE.search(n)]
    repro_diffs = [n for n in diffs if n not in fix_diffs and n != proposed]
    checks: list[common.Check] = []

    if patch is None and repro_diffs:
        submission.set_field('patch', common.CONFLICT, None)
        checks += _fail_both(
            'config.patch',
            f'Reproduction diff {_quote(repro_diffs[0])!r} is attached, but'
            ' "patch" is null.',
            f'Name the diff {REPRO_PATCH_NAME} and set "patch":'
            f' "{REPRO_PATCH_NAME}", or remove it.',
        )
    elif patch is None:
        submission.set_field('patch', common.NONE, None)
    elif patch != REPRO_PATCH_NAME:
        submission.set_field('patch', common.INVALID, None)
        checks += _fail_both(
            'config.patch',
            f'Reproduction patch must be named {REPRO_PATCH_NAME}, got'
            f' {_quote(patch)!r}.',
            f'Rename the reproduction patch to {REPRO_PATCH_NAME}.',
        )
    elif patch not in names:
        submission.set_field('patch', common.CONFLICT, patch)
        checks += _fail_both(
            'config.patch',
            f'"patch" names {REPRO_PATCH_NAME}, but it is not attached.',
            f'Attach {REPRO_PATCH_NAME} or set "patch": null.',
        )
    else:
        submission.set_field('patch', common.VALUE, patch)

    if proposed is None and fix_diffs:
        submission.set_field('proposed_fix', common.CONFLICT, None)
        checks += _fail_both(
            'config.proposed_fix',
            f'Proposed fix {_quote(fix_diffs[0])!r} is attached, but'
            ' "proposed_fix" is null.',
            f'Set "proposed_fix": "{_quote(fix_diffs[0])}" or remove the diff.',
        )
    elif proposed is None:
        submission.set_field('proposed_fix', common.NONE, None)
    elif not (
        proposed.lower().endswith(PATCH_SUFFIXES)
        and FIX_PATCH_RE.search(proposed)
    ):
        submission.set_field('proposed_fix', common.INVALID, None)
        checks += _fail_both(
            'config.proposed_fix',
            'Proposed fix must be a .diff/.patch with `fix` in its name, got'
            f' {_quote(proposed)!r}.',
            'Rename the proposed fix to contain `fix` (e.g. fix.diff).',
        )
    elif proposed not in names:
        submission.set_field('proposed_fix', common.CONFLICT, proposed)
        checks += _fail_both(
            'config.proposed_fix',
            f'"proposed_fix" names {_quote(proposed)!r}, but it is not'
            ' attached.',
            f'Attach {_quote(proposed)} or set "proposed_fix": null.',
        )
    else:
        submission.set_field('proposed_fix', common.VALUE, proposed)
    return checks


def _check_operating_system(
    submission: common.Submission, values: Mapping[str, Any], crash_text: str
) -> list[common.Check]:
    """Checks `operating_system` names and their fit with `environment`.

    Names are turned into one standard spelling (e.g. "mac" -> "macOS").
    Mobile environments must list their OS; desktop ones must list a
    desktop OS. Linux is preferred because automated reproduction runs
    fastest there. Paths in the crash log give a hint about the real OS.
    """
    env = values['environment']
    os_map = dict(CANONICAL_OS_PAIRS)
    os_list: list[str] = []
    invalid: list[str] = []
    for part in values['operating_system']:
        canonical = os_map.get(part.strip().lower())
        if canonical is None:
            invalid.append(part)
        elif canonical not in os_list:
            os_list.append(canonical)
    if invalid:
        submission.set_field('operating_system', common.INVALID, None)
        return _fail_both(
            'config.operating_system',
            f'Unrecognized operating system name(s): {_quote(invalid)}.',
            'Use: Linux, Windows, macOS, Android, iOS, ChromeOS, Fuchsia.',
        )
    required_os = {
        'chrome-android': 'Android',
        'android-webview': 'Android',
        'chrome-ios': 'iOS',
        'chromeos': 'ChromeOS',
    }.get(env)
    is_desktop = env in common.DESKTOP_ENVIRONMENTS
    if required_os is not None and required_os not in os_list:
        submission.set_field('operating_system', common.CONFLICT, os_list)
        return [
            common.Check(
                'config.operating_system',
                common.FAIL,
                f'Environment "{env}" requires {required_os}, but'
                f' "operating_system" is {", ".join(os_list)}.',
                fix=f'Add "{required_os}" to "operating_system".',
                axis=common.AXIS_QUALITY,
            )
        ]
    if is_desktop and all(name in ('Android', 'iOS') for name in os_list):
        submission.set_field('operating_system', common.CONFLICT, os_list)
        return _fail_both(
            'config.operating_system',
            f'Desktop environment "{env}" conflicts with mobile operating'
            f' system: {", ".join(os_list)}.',
            'Use a mobile environment, or list a desktop OS (Linux preferred).',
        )

    submission.set_field('operating_system', common.VALUE, os_list)
    checks = [
        common.Check(
            'config.operating_system',
            common.PASS,
            f'Operating system: {", ".join(os_list)}.',
            axis=common.AXIS_QUALITY,
        )
    ]
    if is_desktop:
        if 'Linux' not in os_list and any(n in MANUAL_ONLY_OS for n in os_list):
            checks.append(
                _manual(
                    'config.operating_system',
                    f'Operating system {", ".join(os_list)} routes to manual'
                    ' verification.',
                )
            )
        elif 'Linux' not in os_list:
            checks.append(
                common.Check(
                    'config.os_linux_preferred',
                    common.INFO,
                    'Desktop report does not list Linux; automated reproduction'
                    ' runs fastest on Linux.',
                    fix='Add "Linux" if the bug also reproduces there.',
                    axis=common.AXIS_QUALITY,
                )
            )
    # Guess the OS from file paths in the crash log. This is only a hint
    # (INFO), since build paths can come from another machine.
    detected = []
    if WINDOWS_PATH_RE.search(crash_text):
        detected.append('Windows')
    if '/Users/' in crash_text:
        detected.append('macOS')
    if '/home/' in crash_text:
        detected.append('Linux')
    extra = [name for name in detected if name not in os_list]
    if extra:
        checks.append(
            common.Check(
                'config.os_stack_paths',
                common.INFO,
                f'Crash log paths reference {", ".join(extra)}, which'
                ' "operating_system" does not list.',
                fix=(
                    'Confirm "operating_system" lists where the PoC reproduces'
                    ' (build paths from other machines can be ignored).'
                ),
                axis=common.AXIS_QUALITY,
            )
        )
    return checks


def _check_versions(
    submission: common.Submission, values: Mapping[str, Any]
) -> list[common.Check]:
    """Checks the `arch` and `chrome_version` values.

    Chrome versions have four numbers (x.x.x.x). For d8, a V8 version with
    three or four numbers is also accepted, since that is what
    `d8 --version` prints.
    """
    checks: list[common.Check] = []
    arch = values['arch']
    if arch in ARCHES:
        submission.set_field('arch', common.VALUE, arch)
    else:
        submission.set_field('arch', common.INVALID, None)
        checks += _fail_both(
            'config.arch',
            f'Unknown architecture "{_quote(arch)}".',
            f'Use one of: {", ".join(ARCHES)}.',
        )
    version = values['chrome_version'].strip()
    is_d8 = values['environment'] == 'd8'
    pattern = V8_VERSION_RE if is_d8 else CHROME_VERSION_RE
    if pattern.fullmatch(version):
        submission.set_field('chrome_version', common.VALUE, version)
    else:
        submission.set_field('chrome_version', common.INVALID, None)
        expected = 'x.x.x or x.x.x.x' if is_d8 else 'x.x.x.x'
        checks += _fail_both(
            'config.chrome_version',
            f'"chrome_version" must look like {expected}, got'
            f' "{_quote(version)}".',
            'Copy the exact version from chrome://version (or `d8 --version`).',
        )
    return checks


def _crash_log_tip(message: str, fix: str) -> list[common.Check]:
    """Returns an INFO check with advice about the crash log."""
    # A crash log speeds up triage but automated reproduction never reads it,
    # so every crash_log problem is a recommendation, never a blocker.
    return [
        common.Check(
            'config.crash_log',
            common.INFO,
            message,
            fix=fix,
            axis=common.AXIS_QUALITY,
        )
    ]


def _check_crash_log(
    submission: common.Submission,
    values: Mapping[str, Any],
    names: frozenset[str],
    contents: Mapping[str, str],
) -> list[common.Check]:
    """Checks the crash log named by `crash_log`, as advice only.

    A good crash log is attached and "symbolized": its stack frames show
    function names, not just raw memory addresses. Problems here never
    block the report.
    """
    name = values['crash_log']
    if name is None:
        unnamed = _crash_log_candidates(names)
        if unnamed:
            submission.set_field('crash_log', common.CONFLICT, None)
            return _crash_log_tip(
                f'{_quote(unnamed[0])!r} looks like a crash log, but'
                ' "crash_log" is null.',
                f'Set "crash_log": "{_quote(unnamed[0])}" so triage finds the'
                ' trace.',
            )
        submission.set_field('crash_log', common.NONE, None)
        return _crash_log_tip(
            'No crash log named; that is fine for bugs that do not crash.',
            'If the bug crashes, attach the symbolized trace (e.g. asan.txt)'
            ' and set "crash_log".',
        )
    if name not in names:
        submission.set_field('crash_log', common.CONFLICT, name)
        return _crash_log_tip(
            f'"crash_log" names {_quote(name)!r}, which is not attached.',
            f'Attach {_quote(name)} or fix the name in "crash_log".',
        )
    text = contents[name]
    # Needs a crash header and a named frame, and no unknown frames or
    # leftover template text.
    symbolized = (
        bool(CRASH_HEADER_RE.search(text))
        and bool(SYMBOLIZED_FRAME_RE.search(text))
        and not UNSYMBOLIZED_MODULE_RE.search(text)
        and not PLACEHOLDER_RE.search(text)
    )
    if not symbolized:
        submission.set_field('crash_log', common.INVALID, None)
        return _crash_log_tip(
            f'{_quote(name)} lacks a recognized crash header or has'
            ' unsymbolized stack frames.',
            'If you can, attach a fully symbolized trace with function names'
            ' and file locations.',
        )
    submission.set_field('crash_log', common.VALUE, name)
    return _pass_both(
        'config.crash_log', f'{_quote(name)} is a symbolized crash trace.'
    )


def _check_context(
    submission: common.Submission,
    values: Mapping[str, Any],
    report: str | None,
) -> list[common.Check]:
    """Checks `preconditions` and `user_interaction` against the report.

    Each must be JSON null or a real description, not "none" or template
    text. A simple text search of the report flags a null value when the
    report mentions host or policy setup, or a user action such as a click.
    """
    steps_text = ''
    # Only look for user actions in the "Steps to reproduce" part and after,
    # so words like "click" elsewhere in the report do not count.
    if report is not None:
        lines = report.splitlines()
        start = next(
            (i for i, line in enumerate(lines) if STEPS_HEADER_RE.match(line)),
            None,
        )
        if start is not None:
            steps_text = '\n'.join(lines[start:])
    conflicts = (
        (
            'preconditions',
            report is not None and ENV_PRECONDITION_CONFLICT_RE.search(report),
            'report steps need host or policy setup',
        ),
        (
            'user_interaction',
            USER_GESTURE_STEPS_RE.search(steps_text),
            'report steps describe a user gesture',
        ),
    )
    problems: list[str] = []
    for key, conflicting, reason in conflicts:
        value = values[key]
        if value is None and conflicting:
            submission.set_field(key, common.CONFLICT, None)
            problems.append(f'"{key}" is null, but {reason}')
        elif value is None:
            submission.set_field(key, common.NONE, None)
        elif (
            not value.strip()
            or value.strip().lower() in NONE_LIKE_STRINGS
            or PLACEHOLDER_RE.search(value)
        ):
            submission.set_field(key, common.INVALID, None)
            problems.append(
                f'"{key}" must be null or a concrete description, got'
                f' "{_quote(value)}"'
            )
        else:
            submission.set_field(key, common.VALUE, value.strip())
    if problems:
        return _fail_both(
            'config.context',
            '; '.join(problems) + '.',
            'Use JSON null when nothing is needed; otherwise describe the setup'
            ' or gesture.',
        )
    return [
        common.Check(
            'config.context',
            common.PASS,
            'Preconditions and user interaction are stated.',
            axis=common.AXIS_QUALITY,
        )
    ]


def _vrp_flag_problems(
    data: Mapping[str, Any], values: Mapping[str, Any], names: frozenset[str]
) -> list[str]:
    """Lists problems in the fields of vrp-flag.json.

    The rules come from docs/security/vrp-faq.md. The file must agree with
    repro.json on the PoC name and the Chrome version.

    Returns:
        A list of problem messages; empty when the file is valid.
    """
    problems: list[str] = []
    if data.get('flag') not in VRP_FLAG_KINDS:
        problems.append('"flag" must be "read" or "write"')
    params = data.get('params')
    if not isinstance(params, dict):
        return problems + ['"params" must be an object']
    if params.get('process') not in VRP_PROCESSES:
        problems.append(f'"params.process" must be one of {VRP_PROCESSES}')
    poc = params.get('poc')
    if not isinstance(poc, str) or poc not in names:
        problems.append('"params.poc" must name an attached file')
    elif poc != values['poc']:
        problems.append(
            f'"params.poc" ({_quote(poc)}) must equal "poc" in {_CONFIG}'
        )
    version = params.get('chrome_version')
    if not isinstance(version, str) or not CHROME_VERSION_RE.fullmatch(version):
        problems.append('"params.chrome_version" must look like x.x.x.x')
    elif version != values['chrome_version']:
        problems.append(
            f'"params.chrome_version" must equal "chrome_version" in {_CONFIG}'
        )
    os_name = params.get('os')
    os_names = dict(VRP_OS_ALIASES) | dict(CANONICAL_OS_PAIRS)
    if not isinstance(os_name, str) or os_name.lower() not in os_names:
        problems.append(
            '"params.os" must be win, mac, linux, android, or chromeos'
        )
    if params.get('arch') not in ARCHES:
        problems.append(f'"params.arch" must be one of {ARCHES}')
    command_line = params.get('command_line')
    if not isinstance(command_line, list) or not all(
        isinstance(item, str) for item in command_line
    ):
        problems.append('"params.command_line" must be a list of strings')
    return problems


def _check_vrp_flag(
    submission: common.Submission,
    values: Mapping[str, Any],
    names: frozenset[str],
    contents: Mapping[str, str],
) -> list[common.Check]:
    """Checks the optional VRP flag proof file (vrp-flag.json).

    Attaching the file is enough to mark a VRP flag report; no config key
    names it. Such reports are always checked by a human, so the result is
    marked manual. The ASan build PoC (poc-asan.html) must also be attached.
    Returns no results when the file is not attached.
    """
    vrp_file = common.VRP_FLAG_FILE_NAME
    if vrp_file not in names:
        return []
    problems: list[str] = []
    data, error = common.parse_json_object(contents[vrp_file])
    if data is None:
        problems.append(f'{vrp_file}: {error}')
    else:
        problems += _vrp_flag_problems(data, values, names)
    if VRP_ASAN_POC not in names:
        problems.append(f'attach {VRP_ASAN_POC} (the ASan build PoC)')
    checks = [
        _manual(
            'config.vrp_flag',
            'VRP flag reports are verified by a human, not automatically.',
        )
    ]
    if problems:
        submission.set_field('vrp_flag', common.INVALID, None)
        checks.append(
            common.Check(
                'config.vrp_flag',
                common.FAIL,
                '; '.join(problems) + '.',
                fix='Follow the vrp-flag.json format in the Chrome VRP FAQ.',
                axis=common.AXIS_QUALITY,
            )
        )
    else:
        submission.set_field('vrp_flag', common.VALUE, vrp_file)
        checks.append(
            common.Check(
                'config.vrp_flag',
                common.PASS,
                f'{vrp_file} is valid.',
                axis=common.AXIS_QUALITY,
            )
        )
    return checks


def _known(submission: common.Submission, key: str) -> tuple[bool, Any]:
    """Returns a field's value if it passed the checks.

    Returns:
        `(True, value)` when the field state is VALUE or NONE, otherwise
        `(False, None)`.
    """
    field = submission.fields.get(key)
    if field and field['state'] in (common.VALUE, common.NONE):
        return True, field['value']
    return False, None


def _suggest_config(submission: common.Submission) -> dict[str, Any]:
    """Builds a pre-filled config template for the researcher.

    Values that passed the checks are kept. Missing ones are guessed from
    the attached files, or left as `<placeholder>` text for the researcher
    to fill in.
    """
    names = [
        n for n in submission.attachment_names if not common.is_metadata_file(n)
    ]
    has_html = any(n.lower().endswith(('.html', '.htm')) for n in names)
    has_server = 'server.py' in names

    def pick(key: str, fallback: Any) -> Any:
        """Returns the checked value for `key`, or `fallback` if none."""
        ok, value = _known(submission, key)
        return value if ok else fallback

    # Guess d8 only for a lone poc.js; anything with HTML or a server is
    # a browser test.
    env = pick(
        'environment',
        'd8'
        if 'poc.js' in names and not has_html and not has_server
        else 'chrome-desktop',
    )
    # d8 always needs the security-PoC flag first; the agent adds
    # `--allow-natives-syntax` when the PoC calls `%Foo()`.
    default_flags = ['--run-as-security-poc'] if env == 'd8' else []
    default_poc = next(
        (n for n in ('poc.html', 'index.html', 'poc.js') if n in names),
        None if has_server else '<poc.html|poc.js>',
    )
    ok, server_file = _known(submission, 'server')
    if not ok:
        server_file = 'server.py' if has_server else None
    template: dict[str, Any] = {
        'version': common.CONFIG_VERSION,
        'environment': env,
        'binary': pick('binary', dict(ENVIRONMENT_BINARIES).get(env)),
        'build': pick('build', '<asan|release>'),
        'flags': pick('flags', default_flags),
        'poc': pick('poc', default_poc),
        'server': (
            {'file': server_file, 'port': REQUIRED_PORT}
            if server_file
            else None
        ),
        'patch': pick(
            'patch', REPRO_PATCH_NAME if REPRO_PATCH_NAME in names else None
        ),
        'proposed_fix': pick(
            'proposed_fix',
            next(
                (
                    n
                    for n in names
                    if n.lower().endswith(PATCH_SUFFIXES)
                    and FIX_PATCH_RE.search(n)
                ),
                None,
            ),
        ),
        'operating_system': pick('operating_system', ['Linux']),
        'arch': pick('arch', '<x64|arm64|x86|arm>'),
        'chrome_version': pick('chrome_version', '<x.x.x.x>'),
        'crash_log': pick(
            'crash_log', next(iter(_crash_log_candidates(names)), None)
        ),
        'preconditions': pick('preconditions', None),
        'user_interaction': pick('user_interaction', None),
    }
    # Keep any environment-specific values the researcher already wrote.
    config = common.load_repro_config(submission)
    raw = config.raw if config is not None else {}
    for key in dict(ENVIRONMENT_REQUIRED_KEYS).get(env, ()):
        template[key] = raw[key] if _has_env_value(raw, key) else f'<{key}>'
    return template


def _run_check(submission: common.Submission) -> list[common.Check]:
    """Runs all checks on `repro.json` and records each field.

    If the file is missing, broken or fails any check, a pre-filled
    template is attached to the result so the researcher can fix it.
    """
    config = common.load_repro_config(submission)
    if config is None:
        submission.suggested_repro_config = _suggest_config(submission)
        return _fail_both(
            'config.present',
            f'No {_CONFIG} attached; automated reproduction reads every machine'
            ' detail from it.',
            f'Attach {_CONFIG} (template below) with the environment, binary,'
            ' build, flags, and PoC file names.',
        )
    if config.error is not None:
        submission.suggested_repro_config = _suggest_config(submission)
        return _fail_both(
            'config.valid',
            f'{_CONFIG} is unusable: {config.error}.',
            f'Fix {_CONFIG} so it is a JSON object (template below).',
        )

    checks: list[common.Check] = []
    env = config.raw.get('environment')
    env_keys = (
        dict(ENVIRONMENT_REQUIRED_KEYS).get(env, ())
        if isinstance(env, str)
        else ()
    )
    unknown = sorted(
        str(k) for k in config.raw if k not in common.CONFIG_KEYS + env_keys
    )
    if unknown:
        checks.append(
            common.Check(
                'config.unknown_keys',
                common.INFO,
                f'{_CONFIG} has key(s) the checker ignores: {_quote(unknown)}.',
                fix='Remove them or check the spelling of required keys.',
                axis=common.AXIS_QUALITY,
            )
        )
    # Stop early on missing keys or wrong types: the detailed checks below
    # assume every key is present with the right type.
    if not config.valid:
        for key in config.missing:
            submission.set_field(key, common.MISSING, None)
        for key in config.wrong_type:
            submission.set_field(key, common.INVALID, None)
        submission.suggested_repro_config = _suggest_config(submission)
        return checks + _fail_both(
            'config.valid',
            f'{_CONFIG}: ' + '; '.join(config.problems) + '.',
            'Every key in the template must be present; use null for "none".',
        )

    values = config.values
    names = frozenset(submission.attachment_names)
    contents = {item.name: item.content for item in submission.files}
    crash_text = contents.get(values['crash_log'] or '', '')
    poc_text = contents.get(values['poc'] or '')
    checks += [
        common.Check(
            'config.valid',
            common.PASS,
            f'{_CONFIG} has every required key.',
            axis=common.AXIS_QUALITY,
        )
    ]
    checks += _check_environment(submission, config)
    checks += _check_binary(submission, values)
    checks += _check_build(submission, values, crash_text)
    checks += _check_flags(submission, values, poc_text)
    checks += _check_entry(submission, values, names)
    server = values['server']
    # Automated reproduction only opens http://localhost:8000/.
    if server is not None:
        if server['port'] == REQUIRED_PORT:
            checks.append(
                common.Check(
                    'config.server_port',
                    common.PASS,
                    f'Server port is {REQUIRED_PORT}.',
                    axis=common.AXIS_REPRO,
                )
            )
        else:
            checks.append(
                common.Check(
                    'config.server_port',
                    common.FAIL,
                    f'"server.port" is {server["port"]}; automated'
                    ' reproduction only'
                    f' opens http://localhost:{REQUIRED_PORT}/.',
                    fix=(
                        f'Serve on port {REQUIRED_PORT} and set "port":'
                        f' {REQUIRED_PORT}.'
                    ),
                    axis=common.AXIS_REPRO,
                )
            )
    checks += _check_patches(submission, values, names)
    checks += _check_operating_system(submission, values, crash_text)
    checks += _check_versions(submission, values)
    checks += _check_crash_log(submission, values, names, contents)
    checks += _check_context(submission, values, submission.report)
    checks += _check_vrp_flag(submission, values, names, contents)

    # Record which kind of PoC runs. A server takes priority over `poc`.
    poc = (values['poc'] or '').lower()
    if server is not None:
        repro_type = 'poc.py'
    elif poc.endswith(('.html', '.htm')):
        repro_type = 'poc.html'
    elif poc.endswith(('.js', '.mjs')):
        repro_type = 'poc.js'
    else:
        repro_type = None
    submission.set_field(
        'repro_type',
        common.VALUE if repro_type else common.MISSING,
        repro_type,
    )
    if any(c.status == common.FAIL for c in checks):
        submission.suggested_repro_config = _suggest_config(submission)
    return checks


def check(submission: common.Submission) -> list[common.Check]:
    """Checks `repro.json` and never crashes on deep nesting.

    A hostile or odd JSON file can be nested deep enough to hit Python's
    recursion limit. That case becomes a "needs judgment" result for a
    human instead of a crash.
    """
    try:
        return _run_check(submission)
    except RecursionError:
        return [
            common.Check(
                'config.recursion_limit',
                common.UNKNOWN,
                f'{_CONFIG} check hit the Python recursion limit; review'
                ' manually.',
                axis=common.AXIS_QUALITY,
                manual=True,
            )
        ]


def main(argv: Sequence[str] | None = None) -> int:
    """Runs this check alone from the command line."""
    return common.run_cli(check, argv=argv, description=__doc__)


if __name__ == '__main__':
    sys.exit(main())
