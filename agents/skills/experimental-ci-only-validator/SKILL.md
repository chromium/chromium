---
name: experimental-ci-only-validator
description: >-
  Checks if a Chromium test suite is barred by `ci_only = True` configuration
  on coverage trybots, which prevents the test suite from running automatically
  on Gerrit CQ runs and can cause underreported code coverage.
---

# CI-Only Validator Skill

This skill provides a quick way to check if a test suite has been configured as
`ci_only = True` on the code coverage trybots.

______________________________________________________________________

## When to Use This Skill

Use this skill when:

- You are triaging an underreported code coverage bug on Gerrit.
- You have already mapped the target source or test file to its parent
  executable test suite (e.g., using `TestSuiteMapper`).
- You suspect that the test suite is configured on the coverage builders but is
  not executing in tryjobs.

______________________________________________________________________

## How to Use This Skill

If you have identified the test suite name (e.g., `chrome_junit_tests`), run the
quick check tool:

```bash
python3 tools/code_coverage/ci_only_validator.py <test_suite_name>
```

### Checking at a Past Revision (Historical Triage)

To verify the `ci_only` status of a test suite at a specific historical point
(e.g., the revision of a Gerrit CL), pass the `--revision` (or `-r`) flag:

```bash
python3 tools/code_coverage/ci_only_validator.py <test_suite_name> --revision <commit_hash_or_branch>
```

This is highly recommended for historical triaging because configurations on
main might have changed since the CL was reviewed.

### Interpreting the Results

The tool will scan CQ coverage trybots and coverage-specific trybots generated
under `infra/config/generated/builders/try/` and report their configuration for
the target test suite:

- **`ci_only = True`**: The test suite is barred from executing on this trybot
  by default during CQ runs. Gerrit coverage details for tests in this suite
  will not be generated automatically unless the developer explicitly requests
  them via a Gerrit footer.
- **`ci_only = False`**: The test suite is allowed to run on the trybot in CQ.
- **`Warning: Test suite ... was not found...`**: The test suite is not
  configured to run on any coverage trybots, meaning no coverage details are
  collected on CQ at all.

### Example Runs

#### 1. Checking Current Configuration (HEAD)

```bash
python3 tools/code_coverage/ci_only_validator.py chrome_junit_tests
```

Output:

```text
Checking test suite 'chrome_junit_tests' across 30 coverage trybots...
  Trybot: android-code-coverage          | Builder: chromium.coverage:android-code-coverage          | ci_only = True
  Trybot: android-desktop-x64-rel        | Builder: chromium.android.desktop:android-desktop-x64-rel-15-tests | ci_only = False
  Trybot: android-x86-code-coverage      | Builder: chromium.coverage:android-x86-code-coverage      | ci_only = True
  Trybot: android-x86-rel                | Builder: chromium.android:android-10-x86-nofieldtrial-rel | ci_only = True
```

This indicates that `chrome_junit_tests` is barred by `ci_only = True` in
standard CQ at HEAD.

#### 2. Checking Historical Configuration (at a revision)

```bash
python3 tools/code_coverage/ci_only_validator.py chrome_junit_tests --revision verify-junit-coverage
```

Output:

```text
Checking test suite 'chrome_junit_tests' across 30 coverage trybots at revision verify-junit-coverage...
  Trybot: android-code-coverage          | Builder: chromium.coverage:android-code-coverage          | ci_only = False
  Trybot: android-desktop-x64-rel        | Builder: chromium.android.desktop:android-desktop-x64-rel-15-tests | ci_only = False
  Trybot: android-x86-code-coverage      | Builder: chromium.coverage:android-x86-code-coverage      | ci_only = False
  Trybot: android-x86-rel                | Builder: chromium.android:android-10-x86-nofieldtrial-rel | ci_only = False
```

This indicates that on the `verify-junit-coverage` branch, the `ci_only`
restriction has been successfully disabled.
