# Chrome for Android Popups Tests (`chrome/android/javatests/.../popups`)

Guidelines and policies for AI agents interacting with or modifying tests in this directory.

## Flaky Test Disablement Policy

When encountering a flaky or failing test:

- **Disable individual tests only:** Only disable the specific relevant test method (e.g., applying `@DisableIf` or `@DisabledTest` directly to the individual `@Test` method).
- **Never disable the entire test suite:** Do not disable an entire test class/suite (e.g., adding class-level `@DisableIf` or `@DisabledTest` annotations), as disabling the entire suite eliminates test coverage for all other tests.
- **Disable on as few bots as possible:** Restrict the disablement to the configurations where the test actually fails (e.g., `@DisableIf` with a specific SDK level, device type, or build configuration) rather than using an unconditional `@DisabledTest`, so that coverage is preserved everywhere else.

## Bug Creation Rules

- **Group by root cause:** File one tracking bug per root cause, not per file or per suite. Tests that fail for the same underlying reason share a bug; tests that fail for different reasons get separate bugs, even within the same file.
- **Provide actionable information:** Every bug must include at least one hypothesis for the root cause, supported by evidence (failure logs, stack traces, affected bots and configurations, flake rate).
- **Identify regressions:** If the failure is a regression, include the culprit CL range (last known good and first known bad revisions).
- **Set priority by lost coverage:** If a test is disabled on all bots, file the bug as P0/P1, because production coverage is lost entirely. Lower priorities are only appropriate when the test still runs on some configurations.

## Reviewers and Bug Escalation

For any bug or Gerrit CL that modifies or disables tests in this directory:
- CC all owners listed in the `OWNERS` file of this directory on the bug.
- Add them as reviewers or CCs on the corresponding Gerrit CL.
