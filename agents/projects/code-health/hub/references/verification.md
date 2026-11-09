# Shared Build & Test Verification Workflow

Delegate verification to a `flash` `self` subagent running
`tools/autotest.py --run-related` (with an `autoninja` compile fallback).

## 1. When to Skip

List files changed on the whole branch (so earlier commits aren't missed) with
`if base=$(git merge-base origin/main HEAD 2>/dev/null); then git diff --name-only "$base"; else git diff --name-only HEAD; fi`.
Run verification if any `.cc`, `.cpp`, `.c`, `.h`, `.m`, `.mm`, `.java`, `.kt`,
`.rs`, `.gn`, `.gni`, `.mojom`, `.proto`, or `.aidl` file changed, or Android
resource XML under `/res/` or `AndroidManifest.xml`. Otherwise skip verification
and proceed to the next step. Metrics XML under `tools/metrics/` and other
non-code XML or Markdown files don't count.

## 2. Invocation Protocol (Code Changes)

Call `invoke_subagent` with:

- `TypeName`: `"self"`
- `Model`: `"flash"`
- `Role`: `"Build and Test Runner"`
- `Workspace`: `"inherit"`

### Subagent Prompt Template

```text
Build and test the modified code files. Run from the repository root (cwd).

Modified code files:
<list of files from Section 1 that match the code/resource types above>

Steps:
1. Select an existing out/<dir> containing both args.gn and build.ninja.
   Check, in order: $CHROMIUM_OUT_DIR (if set), out/current_link (if present),
   then other existing out/* dirs (out/x64, out/Debug, out/Default,
   out/Release, ...). Read `target_os` from args.gn (default: host OS).
   - Require `target_os = "android"` if any .java, .kt, .aidl, Android
     resource XML, `*_android.*`, `/android/`, or `clank/` file changed.
   - Require the matching target_os (win, mac, ios, chromeos, linux) if any
     platform-specific file changed: `*_win.*`, `/win/`, `*_mac.*`, `/mac/`,
     `*_ios.*`, `/ios/`, .mm, `*_chromeos.*`, `/chromeos/`, `*_ash.*`,
     `/ash/`, `*_linux.*`, or `/linux/`.
   - For cross-platform changes with none of the above, prefer a desktop
     out/<dir>, falling back to an existing Android out/<dir>.
   Never fall back to an incompatible platform. If no compatible out/<dir>
   exists, do NOT create one; return status "no_out_dir".
2. Never run `ninja` directly. Use `autoninja` without -j/-l flags.
3. Run tests, keyed off the selected <out_dir>/args.gn. Wait for background
   commands to complete; do not poll.
   - Desktop (not `target_os = "android"`):
     a. vpython3 tools/autotest.py -C <out_dir> --run-related --quiet > out/run-tests.log 2>&1
     b. If out/run-tests.log reports "No associated test files found.", fall
        back to: autoninja -C <out_dir> chrome > out/run-tests.log 2>&1
        (iOS has no `chrome` target, so on `target_os = "ios"` return status
        "no_out_dir" when no associated tests are found.)
   - Android (`target_os = "android"`): never run instrumentation
     APKs or touch attached devices/emulators.
     a. Discover targets without running:
        vpython3 tools/autotest.py -C <out_dir> --run-related --dry-run
     b. Keep only host Robolectric targets (*_junit_tests). Skip all device
        APK / instrumentation targets (names don't always end in _test_apk).
     c. If any *_junit_tests targets matched, run:
        vpython3 tools/autotest.py -C <out_dir> --run-related --target <t1> [--target <t2> ...] --quiet > out/run-tests.log 2>&1
     d. Otherwise, fall back to compile-only with autoninja on the APK target:
        `chrome_apk` if <out_dir>/build.ninja defines it (or clank/java/BUILD.gn
        exists), else `chrome_public_apk`:
        autoninja -C <out_dir> <apk_target> > out/run-tests.log 2>&1
        If a test file itself was modified, also add any matched test target
        to that autoninja call.
4. Inspect failures in out/run-tests.log (first error/failure lines only; never
   paste the full log) and output ONLY this JSON block:

{
  "status": "pass" | "fail" | "build_fail" | "no_out_dir",
  "command": "<exact command run or none>",
  "log_file": "out/run-tests.log",
  "summary": "<one-line result or first failing test/error>"
}
```

## 3. Handling Results

- `pass`: Report the command and proceed to the next step.
- `no_out_dir`: Inform the user, then use `ask_question` to ask whether to
  proceed to the next step without local build verification or stop.
- `fail` / `build_fail`: Show the `summary` and the `out/run-tests.log` path,
  fix any regression caused by the cleanup, re-run `format_and_lint.py`, and
  re-run verification. Escalate to the user if the cause is unclear.
