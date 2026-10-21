---
name: base/check-terminating-null-windows-environment-block
rubric: correctness
severity: should_fix
---

**Rule.** When parsing or iterating over Windows environment blocks, do not rely
solely on buffer or span bounds to terminate the loop over variables; explicitly
check for the terminating null character (such as `remaining[0] != L'\0'` or
`env[idx] != L'\0'`).

**Why.** Win32 environment blocks are double-null-terminated lists of
null-terminated strings. Relying solely on buffer or span length without
checking for the terminating null treats the trailing null as an environment
variable entry, producing invalid empty keys or corrupting the block for child
processes.

## Examples

Bad:

```cpp
// BAD: Iterating solely on span/buffer bounds parses the final null as an entry.
while (!remaining.empty()) {
  NativeEnvironmentStringView key;
  size_t line_len = ParseEnvLine(remaining, &key);
  remaining = remaining.subspan(line_len);
}
```

Good:

```cpp
// GOOD: Explicitly check for the terminating null character.
while (!remaining.empty() && remaining[0] != L'\0') {
  NativeEnvironmentStringView key;
  size_t line_len = ParseEnvLine(remaining, &key);
  remaining = remaining.subspan(line_len);
}
```
