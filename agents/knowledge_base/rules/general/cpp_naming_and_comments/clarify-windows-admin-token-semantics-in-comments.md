---
name: general/clarify-windows-admin-token-semantics-in-comments
rubric: readability
severity: nit
---

**Rule.** When documenting Windows administrator security checks, token
elevation prerequisites, or split-token status (such as
`TokenElevationTypeDefault` or `AccessToken::IsSplitToken()`), explicitly
specify the exact token states and semantics. Clarify that non-split tokens
include accounts like the SYSTEM user or built-in Administrator, and that
split-token queries return true for both unelevated and elevated UAC tokens
rather than indicating admin status.

**Why.** Under Windows User Account Control (UAC), administrator accounts
operate with split-token pairs (limited vs. elevated), whereas accounts like
SYSTEM or the built-in Administrator run with non-split tokens. Vague terms like
'default admin' or undocumented split-token checks lead callers to confuse
split-token presence with unelevated status, current administrator privileges,
or standard UAC accounts.

## Examples

Bad:

```cpp
// Returns `true` if the token is a split UAC token.
bool IsSplitToken() const;

// The directory is created under SystemTemp for security reasons if the caller
// is the default admin to avoid attacks from lower privilege processes.
bool CreateNewTempDirectory(FilePath::StringViewType prefix,
                            FilePath* new_temp_path);
```

Good:

```cpp
// Returns `true` if the token is a split UAC token. It will return true for
// both unelevated UAC and also elevated UAC. This function does not indicate
// whether the token is admin or not, merely that it is split.
bool IsSplitToken() const;

// The directory is created under SystemTemp for security reasons if the caller
// is the default admin (i.e., no split token, such as the SYSTEM user or the
// built-in administrator) to avoid attacks from lower privilege processes.
bool CreateNewTempDirectory(FilePath::StringViewType prefix,
                            FilePath* new_temp_path);
```
