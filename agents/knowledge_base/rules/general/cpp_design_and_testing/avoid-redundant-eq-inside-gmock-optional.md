---
name: general/avoid-redundant-eq-inside-gmock-optional
rubric: readability
severity: nit
---

**Rule.** When asserting that an optional contains a specific value using
GoogleMock's `testing::Optional`, pass the expected value directly (e.g.,
`testing::Optional(expected_value)`). Avoid wrapping the expected value in
`testing::Eq(...)`, as `testing::Optional` implicitly converts bare values to
equality matchers.

**Why.** `testing::Optional` accepts either a nested matcher or a plain value,
treating bare values as implicit equality comparisons. Explicitly wrapping the
value in `testing::Eq(...)` adds redundant boilerplate and visual clutter.

## Examples

Bad:

```cpp
ASSERT_THAT(first_file.Write(0, byte_span),
            testing::Optional(testing::Eq(byte_span.size())));
EXPECT_THAT(maybe_name, testing::Optional(testing::Eq("expected_name")));
```

Good:

```cpp
ASSERT_THAT(first_file.Write(0, byte_span),
            testing::Optional(byte_span.size()));
EXPECT_THAT(maybe_name, testing::Optional("expected_name"));
```
