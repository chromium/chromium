---
name: reenable-test
description: >-
  Guide for investigating, fixing, verifying via TDD, and re-enabling disabled
  or flaky Chromium tests (C++, Android Java, or WebUI). Use this skill when
  asked to deflake, fix, or re-enable a disabled or flaky test case, or to
  verify post-submit stability of a re-enabled test. Don't use for disabling a
  failing test without fixing it (use the disable-test skill instead).
---

# Re-Enabling Flaky or Disabled Chromium Tests

This guide defines the workflow for investigating, root-causing, verifying via
Red-Green Test-Driven Development (TDD), and re-enabling disabled or flaky tests
in Chromium.

- **Pre-Submit Workflow (Steps 1–5)**: Execute when fixing and uploading a CL to
  re-enable a test. Stop after Step 5 and hand off to the user for review/merge.
- **Post-Submit Workflow (Step 6)**: Execute only in a follow-up session after
  confirming the CL has landed on `main`.

## Step 1: Triage History, Bug Context, and Failure Scope

Before modifying code, gather the historical context for why the test was
disabled and where it failed:

1. **Locate the Disablement & Tracking Bug**:
   - Inspect the test file or `testing/buildbot/filters/*.filter` for
     `DISABLED_`, `#define MAYBE_`, `@DisabledTest`, `@DisableIf`,
     `test.skip()`, or `-Suite.Test` filter entries, along with any associated
     `crbug.com/<BUG_ID>` or `b/<BUG_ID>` comment.
2. **Inspect Git Blame & Commit History**:
   - Run `git log -S "<TestName>" -p <path/to/test_file>` to find the commit
     that disabled the test and any prior re-enablement attempts that were
     reverted (including the revert's failing build link and failure reason).
3. **Review Tracking Bug & LUCI History**:
   - Read the tracking bug description and comments to identify the exact
     failure mode (LSan leak, TSan race, timeout, crash, or assertion failure)
     and affected platforms/configurations (Windows, macOS, Linux, ChromeOS
     ASan/LSan, Android).
   - Check recent CI test history using the `luci-test-results` skill
     ([SKILL.md](../../../third_party/depot_tools/agents/skills/luci-test-results/SKILL.md)):
     ```sh
     vpython3 third_party/depot_tools/agents/skills/luci-test-results/scripts/test_history.py \
       --test-substring '<TestSuite#TestName>' --bucket ci
     ```

## Step 2: Red-Green TDD Reproduction & Root-Cause Fix

### Phase 2A: Red Phase (Reproduce the Failure Before Fixing)

Always attempt to reproduce the failure **before** applying a fix so that the
subsequent passing run proves the fix addresses the root cause:

1. **Resolve Build Directory & Test Target**:
   - Identify the active build directory (`<out_dir>`, typically `out/Default`
     or an existing `out/*` directory matching the target `args.gn` such as
     `is_asan=true is_lsan=true` or `is_tsan=true`).
   - Discover the GN `<test_target>` for the test file and compile it:
     ```sh
     gn refs <out_dir> <path/to/test_file>
     autoninja -C <out_dir> <test_target>
     ```
2. **Run the Unfixed Test Locally**:
   - Temporarily re-enable the test (or pass `--gtest_also_run_disabled_tests`).
   - Run 1 iteration first to benchmark single-run wall-clock duration. Use
     `100` iterations for fast unit tests (`<200ms` per run) and `20` iterations
     for slower unit tests (e.g., real-time media streaming or `>=1s` per run),
     browser tests, or instrumentation tests to avoid multi-minute sequential
     test runs. Redirect repeated output to a log file to avoid context bloat:
     - **C++ (GTest) & WebUI (`browser_tests`)**:
       ```sh
       ./<out_dir>/<test_target> \
         --gtest_filter='<TestSuite>.<TestName>' \
         --gtest_also_run_disabled_tests \
         --gtest_repeat=100 \
         --gtest_break_on_failure \
         --test-launcher-retry-limit=0 \
         --test-launcher-bot-mode > /tmp/test_repro.log 2>&1
       ```
     - **Android Java (Robolectric / Instrumentation)**:
       ```sh
       ./<out_dir>/bin/run_<test_target> \
         -f '<TestClass>#<testMethod>' \
         --repeat=20 > /tmp/test_repro.log 2>&1
       ```
   - Verify that the reproduced failure in `/tmp/test_repro.log` matches the
     historical failure signature or stack trace from Step 1 (not an unrelated
     local environment or display error).
3. **When Local Reproduction Is Not Possible**:
   - If the failure requires a bot environment or target OS that cannot run
     directly on the host workstation, first try reproducing and validating via
     the Universal Test Runner ([SKILL.md](../../shared/skills/utr/SKILL.md)),
     which compiles locally and runs the test on remote Swarming bots.
   - You may only bypass pre-fix reproduction if:
     1. Neither local execution nor UTR works (e.g., cross-compiling issues such
        as targeting macOS/iOS from Linux), **or**
     2. You executed the repeated run above and the timing race did not trigger.
   - When pre-fix reproduction is not possible:
     - Explicitly state to the user and in the CL description that the failure
       could not be reproduced locally and why.
     - Ground the fix in a concrete code-level root-cause analysis matched
       against the historical failure log/stack trace from Step 1.
     - As a last resort when UTR cannot be used, upload a WIP/draft CL and
       trigger the target-platform tryjob (`git cl try`) in Step 5 to validate
       the fix.

### Phase 2B: Root-Cause Analysis

Keep code searches scoped to the failing test fixture and the direct module
under test. Once the failure mechanism is identified, do not audit
`third_party/` internals or unrelated callers across the repository.

If the root cause is not immediately clear from the failure log or stack trace,
or if the test passes locally but fails in CQ's `check flakiness` step, consult
[common_flakiness_patterns.md](references/common_flakiness_patterns.md) for
common Chromium testing pitfalls and mitigation strategies.

### Phase 2C: Green Phase (Apply Fix & Clean Up Disablement)

1. Apply the fix and add concise comments explaining non-obvious synchronization
   or teardown ordering.
2. Remove the test disablement and obsolete `TODO(crbug.com/...)` comment:
   - **C++ (GTest)**: Remove `DISABLED_` or `#if ... #define MAYBE_...` blocks
     and restore the test name in `TEST_F` / `TEST_P`. Remove any resolved
     `LeakedDanglingUntriaged` annotations.
   - **Android Java**: Remove `@DisabledTest` or `@DisableIf` (and unused
     imports).
   - **WebUI (Mocha)**: Restore `test.skip()` / `suite.skip()` to `test()` /
     `suite()` and remove `<if expr>` disablement wrappers.
   - **Filter Files**: Remove `-<TestSuite>.<TestName>` entries from
     `testing/buildbot/filters/*.filter`.

## Step 3: Local Stress Verification & Presubmit

1. **Compile and Stress-Test the Re-Enabled Test(s)**:
   - Recompile with `autoninja -C <out_dir> <test_target>` and run the repeated
     command from Phase 2A (`100` iterations for unit tests, `20` for browser /
     instrumentation tests, wrapped in `xvfb-run -a` for headless Linux UI
     tests) to confirm `0` failures.
   - **Retry Bound**: If the test still fails or flakes during verification,
     iterate on the fix at most **3 times** before stopping and escalating the
     remaining failure log to the user.
2. **Run the Entire Test Fixture or `@Batch` Class**:
   - Run all tests in the C++ fixture (`--gtest_filter='<TestSuite>.*'`) or
     Android `@Batch` class (`-f '<TestClass>.*'`) to confirm the re-enabled
     test does not leak state into or break sibling tests.
3. **Run Formatting and Presubmit Checks**:
   ```sh
   git cl format
   git cl presubmit -u --force
   ```

## Step 4: CL Description Formatting

Draft a concise, useful CL description following
[SKILL.md](../../shared/skills/cl-description/SKILL.md) and format it to \<=72
characters per line using
`vpython3 agents/shared/skills/cl-description/scripts/wrap_lines.py <draft_file>`.
Every re-enabled test CL description **must** include all four items below:

1. **Reproduction status**: Explicitly state whether the original failure or
   flake was reproduced locally (e.g.,
   `Reproduced deterministically locally with --gtest_also_run_disabled_tests`)
   or why local reproduction was not possible.
2. **Root cause & fix**: Concisely explain the underlying race, lifecycle bug,
   or state mismatch and how the change resolves it.
3. **Verification**: State how the fix was verified (e.g.,
   `Verified with 100 local iterations (--gtest_repeat=100) and the full <TestSuite> fixture with 0 failures`,
   plus any platform/sanitizer trybots).
4. **Bug reference**: Include `Bug: <BUG_ID>` in the footer (if the test comment
   cites a legacy 6–7 digit Monorail ID, use the migrated Buganizer ID so
   `git cl presubmit` does not warn on `monorail_bug_tag`).

## Step 5: Cross-Platform & Sanitizer Try-Job Verification

1. **Upload the CL and Trigger Scoped Trybots**:
   - Upload the CL with `git cl upload --force` (do not pass `-m` on initial
     upload, which overrides the Gerrit CL description instead of using `HEAD`'s
     commit message; if updating an existing CL description from `HEAD`, run
     `git cl description --new-description=+ --force`).
   - Trigger trybots matching the platforms and sanitizers where the test failed
     in Step 1 (for example: `linux-rel`, `linux-chromeos-rel`, `mac-rel`,
     `win-rel`, `android-arm64-rel`, `android-12-x64-rel`,
     `linux_chromium_asan_rel_ng`, `linux_chromium_tsan_rel_ng`,
     `linux_chromium_chromeos_asan_rel_ng`):
     ```sh
     git cl try -B luci.chromium.try -b <target_builder>
     ```
2. **Verification When Local Reproduction Was Not Possible**:
   - Confirm the CQ Flake Endorser (`check flakiness` step with
     `--gtest_repeat=20`) passes on the target OS trybot, or trigger up to 3–5
     runs of the target OS trybot and confirm `0` failures.
3. **Handoff for Review**:
   - Report the CL link and trybot status to the user. **Stop here** until the
     CL is reviewed and merged.

## Step 6: Post-Submit Verification & Bug Follow-Up (Post-Merge Only)

Execute this step only after confirming the CL has landed on `main` and CI
builders have cycled:

1. **Query Post-Submit CI History**:
   - Run `test_history.py` from `luci-test-results` scoped to `ci` (which also
     prints the canonical `<FULL_RESULTDB_TEST_ID>`):
     ```sh
     vpython3 third_party/depot_tools/agents/skills/luci-test-results/scripts/test_history.py \
       --test-substring '<TestSuite#TestName>' --bucket ci
     ```
   - For aggregate post-submit pass/flake counts since the landing date
     (`<YYYY-MM-DD>`), query `luci.analysis.v1.TestHistory.QueryStats`:
     ```sh
     prpc call analysis.api.luci.app luci.analysis.v1.TestHistory.QueryStats \
       '{"project":"chromium","testId":"<FULL_RESULTDB_TEST_ID>","predicate":{"partitionTimeRange":{"earliest":"<YYYY-MM-DD>T00:00:00Z"},"submittedFilter":"ONLY_SUBMITTED"}}'
     ```
   - Confirm `0` unexpected failures and `0` flaky verdicts across target
     builders.
2. **Post Follow-Up Comment on the Tracking Bug**:
   - URL-encode `<FULL_RESULTDB_TEST_ID>` (replacing `:` with `%3A`, `/` with
     `%2F`, `!` with `%21`, `#` with `%23`) and post the pass count, CL link,
     and LUCI Analysis URL
     (`https://ci.chromium.org/ui/test/chromium/<URL_ENCODED_TEST_ID>`) to the
     tracking bug.
