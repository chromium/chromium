---
name: general/document-pass-condition-in-assertion-free-tests
rubric: readability
severity: nit
---

**Rule.** In tests that lack explicit GoogleTest assertions (such as EXPECT\_\*
or ASSERT\_\*) because the success criterion is simply that an operation
completes without crashing or failing a CHECK/DCHECK assertion, add an
explanatory comment clarifying that the test passes if it does not crash or
assert.

**Why.** Tests without explicit assertions can confuse readers and reviewers
into thinking assertions were accidentally omitted or obscure what invariant is
being tested. Explicitly documenting the pass condition clarifies intent.

## Examples

Bad:

```cpp
TEST(SpanTest, UncheckedExtentConstructor) {
  char buffer[31];
  UNSAFE_BUFFERS(base::span(base::unchecked, buffer, 999u));
}
```

Good:

```cpp
TEST(SpanTest, UncheckedExtentConstructor) {
  char buffer[31];
  // The test passes if this does not crash or `CHECK()`.
  UNSAFE_BUFFERS(base::span(base::unchecked, buffer, 999u));
}
```
