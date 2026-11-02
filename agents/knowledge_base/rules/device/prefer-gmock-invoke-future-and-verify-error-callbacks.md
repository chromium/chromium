---
name: device/prefer-gmock-invoke-future-and-verify-error-callbacks
rubric: testing
severity: should_fix
---

**Rule.** In asynchronous Device API unit and browser tests:

1. Prefer gMock expectations paired with `base::test::TestFuture` and
   `base::test::InvokeFuture` to capture parameters passed to asynchronous Mojo
   or delegate calls instead of writing hand-rolled stateful test fakes or
   draining task queues with `RunUntilIdle()` (and copy non-owning `base::span`
   callback views into owning containers before the callback returns).
2. Place test assertions in the test body after `future.Get()` or
   `future.Take()` without a redundant `future.Wait()` call beforehand.
3. When testing early-return or error paths where an inner mock expects zero
   calls (`Times(0)`), also assert that the caller's outer completion callback
   resolved with the expected error result; when testing bad-message reports,
   intercept and assert the message using `mojo::test::BadMessageObserver`.

**Why.** Draining queues with `RunUntilIdle()` causes flaky test
synchronization; storing non-owning `base::span` views in a `TestFuture` dangles
once the backing Mojo message buffer is freed; assertions placed inside callback
lambdas silently pass if the callback is never invoked; asserting only
`Times(0)` on an inner mock passes even when the outer method drops its response
callback; and unintercepted bad-message reports fail without verifying which
security invariant triggered.

## Examples

Bad:

```cpp
TEST_F(HidConnectionImplTest, WriteInvalidReportIdFails) {
  // BAD: Uses a hand-rolled `last_write_called_` fake + `RunUntilIdle()`, calls
  // redundant `future.Wait()` before `future.Get()`, and does not verify the
  // bad-message string or outer callback on error.
  EXPECT_CALL(mock_connection_, PlatformWrite).Times(0);
  connection_remote_->Write(/*report_id=*/0, data, base::DoNothing());
  base::RunLoop().RunUntilIdle();
}
```

Good:

```cpp
TEST_F(HidConnectionImplTest, WriteSuccessAndErrorCallbackVerified) {
  base::test::TestFuture<scoped_refptr<base::RefCountedBytes>,
                         HidConnection::WriteCallback>
      write_future;
  EXPECT_CALL(mock_connection_, PlatformWrite)
      .WillOnce(base::test::InvokeFuture(write_future));

  base::test::TestFuture<bool> result_future;
  connection_remote_->Write(/*report_id=*/1, data, result_future.GetCallback());

  auto [buffer, callback] = write_future.Take();
  EXPECT_EQ(buffer->as_vector(), expected_report_with_id);
  std::move(callback).Run(/*success=*/true);
  EXPECT_TRUE(result_future.Get());

  // On early-return error path, verify both `Times(0)` and outer callback:
  EXPECT_CALL(mock_connection_, PlatformWrite).Times(0);
  base::test::TestFuture<bool> error_future;
  connection_remote_->Write(/*report_id=*/0, too_large_data,
                            error_future.GetCallback());
  EXPECT_FALSE(error_future.Get());
}
```
