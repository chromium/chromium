---
name: general/request-process-query-limited-information-for-token-queries
rubric: correctness
severity: should_fix
---

**Rule.** When opening a Windows process handle to query its access token or
retrieve basic metadata (such as via OpenProcessToken or
base::win::AccessToken::FromProcess), request PROCESS_QUERY_LIMITED_INFORMATION
instead of PROCESS_QUERY_INFORMATION.

**Why.** Although Microsoft documentation claims PROCESS_QUERY_INFORMATION is
required to open a process token, PROCESS_QUERY_LIMITED_INFORMATION is
sufficient on all supported Windows versions. Requesting limited access rights
adheres to the principle of least privilege and avoids unnecessary
ERROR_ACCESS_DENIED failures when inspecting processes across integrity levels
or under restrictive security descriptors.

## Examples

Bad:

```cpp
bool IsProcessRunningSplitToken(ProcessId process_id) {
  auto process = Process::OpenWithAccess(process_id, PROCESS_QUERY_INFORMATION);
  if (!process.IsValid()) {
    return false;
  }
  std::optional<win::AccessToken> token =
      AccessToken::FromProcess(process.Handle());
  return token && token->IsSplitToken();
}
```

Good:

```cpp
bool IsProcessRunningSplitToken(ProcessId process_id) {
  auto process =
      Process::OpenWithAccess(process_id, PROCESS_QUERY_LIMITED_INFORMATION);
  if (!process.IsValid()) {
    return false;
  }
  std::optional<win::AccessToken> token =
      AccessToken::FromProcess(process.Handle());
  return token && token->IsSplitToken();
}
```
