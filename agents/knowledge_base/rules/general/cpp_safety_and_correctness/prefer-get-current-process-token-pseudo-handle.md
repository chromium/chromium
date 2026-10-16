---
name: general/prefer-get-current-process-token-pseudo-handle
rubric: correctness
severity: should_fix
---

**Rule.** When querying or inspecting Windows access token properties for the
current process (such as via GetTokenInformation), pass the pseudo-handle
GetCurrentProcessToken() directly instead of opening and scoping a handle via
OpenProcessToken(GetCurrentProcess(), ...). Do not add inaccurate defensive
fallback heuristics (such as falling back to IsUserAnAdmin() or
UserAccountControlIsEnabled()) on query failure; return a clean failure value or
false directly.

**Why.** Passing the pseudo-handle GetCurrentProcessToken() directly avoids
handle allocation and ScopedHandle boilerplate while ensuring process-level
queries succeed even during thread impersonation. Token queries via
GetCurrentProcessToken() with TOKEN_QUERY rights do not fail in normal
execution, whereas secondary fallback heuristics are dead code that can
misclassify elevation states (such as misreporting built-in Administrator
accounts running without split tokens).

## Examples

Bad:

```cpp
HANDLE process_token;
if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token)) {
  base::win::ScopedHandle scoped_process_token(process_token);
  DWORD size;
  TOKEN_ELEVATION_TYPE elevation_type;
  if (GetTokenInformation(scoped_process_token.get(), TokenElevationType,
                          &elevation_type, sizeof(elevation_type), &size)) {
    return elevation_type == TokenElevationTypeFull;
  }
}

// Fallback heuristic if token query fails.
return IsUserAnAdmin() && UserAccountControlIsEnabled();
```

Good:

```cpp
DWORD size;
TOKEN_ELEVATION_TYPE elevation_type;
if (GetTokenInformation(GetCurrentProcessToken(), TokenElevationType,
                        &elevation_type, sizeof(elevation_type), &size)) {
  return elevation_type == TokenElevationTypeFull;
}
return false;
```
