---
name: general/log-errors-at-failure-point-in-helpers
rubric: design
severity: should_fix
---

**Rule.** When handling error conditions in helper functions and their callers,
log specific error details directly at the internal point of failure within the
helper function rather than emitting generic error logs in callers. Callers
should handle or propagate the failure without duplicating log output.

**Why.** Logging in caller functions when a helper fails often produces
duplicate log messages if the helper already logged internally, and caller logs
lack specific context regarding which intermediate operation failed. Logging
directly at the exact point of failure within the helper provides actionable
diagnostics and allows callers to cleanly handle or propagate errors without
redundant logging.

## Examples

Bad:

```cpp
// In helper function:
if (!InsertDictionaryKey(message.get(), key, operation)) {
  return nullptr;
}
return SendMessage(message.get());

// In caller:
auto response = MessageForJob(job_label, LAUNCH_KEY_GETJOB);
if (!response) {
  LOG(ERROR) << "PIDForJob: invalid response";
  return -1;
}
```

Good:

```cpp
// In helper function:
if (!InsertDictionaryKey(message.get(), key, operation)) {
  LOG(ERROR) << "InsertDictionaryKey failed";
  return nullptr;
}
auto response = SendMessage(message.get());
if (!response) {
  LOG(ERROR) << "SendMessage failed";
}
return response;

// In caller:
auto response = MessageForJob(job_label, LAUNCH_KEY_GETJOB);
if (!response) {
  // Helper has already logged the specific internal failure.
  return -1;
}
```
