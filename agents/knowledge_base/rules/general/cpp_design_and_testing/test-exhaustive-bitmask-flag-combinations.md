---
name: general/test-exhaustive-bitmask-flag-combinations
rubric: testing
severity: should_fix
---

**Rule.** When writing unit tests for functions or lookup tables that map,
translate, or validate bitmask flag combinations (such as `base::File::FLAG_*`
or compound options), test the exhaustive permutations of supported flag
combinations rather than an arbitrary subset. Additionally, verify that
unsupported or invalid flag permutations are rejected, sanitized, or handled
gracefully.

**Why.** Bitmask translation and mapping functions handle subtle multi-flag
interactions between overlapping modes, dispositions, and access flags. Testing
only a partial sample risks leaving valid compound flag combinations untested or
failing to catch invalid combinations that cause unexpected runtime behavior,
mis-translation, or crashes.

## Examples

Bad:

```cpp
// Only tests an arbitrary subset of supported flag combinations:
constexpr auto kTranslations = base::MakeFixedFlatMap<uint32_t, std::string>({
    {File::FLAG_OPEN | File::FLAG_READ, "r"},
    {File::FLAG_OPEN_ALWAYS | File::FLAG_READ, "r"},
    {File::FLAG_CREATE | File::FLAG_READ, "r"},
    {File::FLAG_OPEN_ALWAYS | File::FLAG_READ | File::FLAG_WRITE, "rw"},
    {File::FLAG_OPEN_ALWAYS | File::FLAG_APPEND, "wa"},
    {File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE, "wt"},
});
for (const auto& [flags, expected_mode] : kTranslations) {
  EXPECT_EQ(TranslateOpenFlags(flags), expected_mode);
}
```

Good:

```cpp
// Exhaustively tests all valid combinations and invalid permutations:
constexpr auto kTranslations = base::MakeFixedFlatMap<uint32_t, std::string>({
    {File::FLAG_OPEN | File::FLAG_READ, "r"},
    {File::FLAG_OPEN_ALWAYS | File::FLAG_READ, "r"},
    {File::FLAG_CREATE | File::FLAG_READ, "r"},
    {File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WRITE, "rw"},
    {File::FLAG_OPEN_ALWAYS | File::FLAG_READ | File::FLAG_WRITE, "rw"},
    {File::FLAG_CREATE | File::FLAG_READ | File::FLAG_WRITE, "rw"},
    {File::FLAG_OPEN | File::FLAG_APPEND, "wa"},
    {File::FLAG_OPEN | File::FLAG_APPEND | File::FLAG_WRITE, "wa"},
    {File::FLAG_OPEN_ALWAYS | File::FLAG_APPEND, "wa"},
    {File::FLAG_OPEN_ALWAYS | File::FLAG_APPEND | File::FLAG_WRITE, "wa"},
    {File::FLAG_CREATE_ALWAYS | File::FLAG_READ | File::FLAG_WRITE, "rwt"},
    {File::FLAG_OPEN_TRUNCATED | File::FLAG_READ | File::FLAG_WRITE, "rwt"},
    {File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE, "wt"},
    {File::FLAG_CREATE_ALWAYS | File::FLAG_APPEND, "wt"},
    {File::FLAG_CREATE_ALWAYS | File::FLAG_APPEND | File::FLAG_WRITE, "wt"},
    {File::FLAG_OPEN_TRUNCATED | File::FLAG_WRITE, "wt"},
});
for (const auto& [flags, expected_mode] : kTranslations) {
  EXPECT_EQ(TranslateOpenFlags(flags), expected_mode);
}
// Also verify invalid/unsupported permutations are rejected:
EXPECT_EQ(TranslateOpenFlags(File::FLAG_OPEN_TRUNCATED | File::FLAG_READ), "");
```
