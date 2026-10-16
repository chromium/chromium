---
name: general/retain-unit-tests-until-function-removal
rubric: testing
severity: should_fix
---

**Rule.** When refactoring, modernizing, or deprecating APIs (such as converting
raw-pointer functions to spans), do not delete unit tests or fuzzer cases for an
API variant while that variant remains in the codebase. When replacing legacy
methods, update existing fuzzers and test suites to exercise the new replacement
methods rather than removing test cases and losing coverage.

**Why.** Deleting tests for existing overloads leaves active code paths untested
and vulnerable to silent regressions. Similarly, removing fuzzer or test cases
during API modernization drops crucial fuzzing and regression coverage for newly
introduced replacement methods.

## Examples

Bad:

```cpp
// 1. Deleting unit tests for an overload that still exists in the codebase:
// (HexEncode(const void*, size_t) remains in string_number_conversions.h)
-  EXPECT_EQ(HexEncode(nullptr, 0), "");
-  EXPECT_EQ(HexEncode(kBytes.data(), sizeof(kBytes)), "01FF02FE038081");

// 2. Deleting fuzzer test cases instead of updating to replacement APIs:
// (Removing legacy ReadData(const char**, size_t*) without testing replacement ReadData())
-case 13: {
-  const char* data_result = nullptr;
-  iter.ReadData(&data_result, &length);
-  break;
-}
```

Good:

```cpp
// 1. Retain unit tests as long as the overload remains in the codebase:
  EXPECT_EQ(HexEncode(nullptr, 0), "");
  EXPECT_EQ(HexEncode(kBytes.data(), sizeof(kBytes)), "01FF02FE038081");

// 2. Update fuzzer cases to exercise the replacement methods:
case 13: {
  std::ignore = iter.ReadData();  // Exercises replacement span-based API
  break;
}
case 14: {
  int read_length = data_provider.ConsumeIntegralInRange(0, kMaxReadLength);
  std::ignore = iter.ReadBytes(static_cast<size_t>(read_length));
  break;
}
```
