---
name: general/test-error-handling-and-failure-metrics
rubric: testing
severity: should_fix
---

**Rule.** When adding or modifying unit tests for operations, APIs, or
error-handling logic, include test coverage for both the success/happy path and
failure/error conditions (such as invalid arguments, access denials, or locked
resources) rather than testing only one outcome. If exercising a success or
error path is impossible in the test environment, add an explanatory comment or
expose test hooks (`Set*CallbackForTesting`) to simulate environmental failures
and verify recovery and telemetry (such as `base::HistogramTester`).

**Why.** Testing only the success path leaves error recovery, retry logic, and
failure telemetry unverified, while testing only error paths leaves primary
functionality unverified. Exercising both outcomes—or explicitly documenting or
injecting environmental constraints—ensures normal operation and error recovery
function reliably without silent regressions.

## Examples

Bad:

```cpp
// 1. Only testing failure/denial cases without verifying the success path:
TEST_F(ElevationServiceTest, RunRecoveryCRXElevated) {
  ULONG_PTR proc_handle = 0;
  EXPECT_EQ(E_ACCESSDENIED,
            elevator.value()->RunRecoveryCRXElevated(
                crx_path.value().c_str(), app_guid, L"57.8.0.1",
                session_id, ::GetCurrentProcessId(), &proc_handle));
}

// 2. Only testing success when error branches, retries, or metrics were added:
TEST_F(FileUtilTest, ReplaceFileSuccess) {
  base::HistogramTester tester;
  EXPECT_TRUE(ReplaceFile(from_path, to_path, /*error=*/nullptr));
  tester.ExpectUniqueSample("Windows.ReplaceFileResult", /*sample=*/0, 1);
}
```

Good:

```cpp
// 1. Testing both failure and success paths (or commenting why untestable):
TEST_F(ElevationServiceTest, RunRecoveryCRXElevated) {
  ULONG_PTR proc_handle = 0;
  EXPECT_EQ(E_ACCESSDENIED,
            elevator.value()->RunRecoveryCRXElevated(
                crx_path.value().c_str(), app_guid, L"57.8.0.1",
                session_id, ::GetCurrentProcessId(), &proc_handle));
  EXPECT_EQ(S_OK,
            elevator.value()->RunRecoveryCRXElevated(
                crx_path.value().c_str(), app_guid, L"1.0.0.0",
                session_id, ::GetCurrentProcessId(), &proc_handle));
}

// 2. Exercising failure via test hooks/environment to verify recovery & telemetry:
TEST_F(FileUtilTest, ReplaceFileOtherErrorsMoveFailed) {
  base::HistogramTester tester;
  File to_file(to_path, File::FLAG_OPEN | File::FLAG_READ);
  ASSERT_TRUE(to_file.IsValid());
  EXPECT_FALSE(ReplaceFile(from_path, to_path, /*error=*/nullptr));
  tester.ExpectUniqueSample("Windows.ReplaceFileResult", /*sample=*/8, 1);
}
```
