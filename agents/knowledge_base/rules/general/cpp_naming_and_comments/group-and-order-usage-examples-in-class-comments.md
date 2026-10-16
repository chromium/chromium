---
name: general/group-and-order-usage-examples-in-class-comments
rubric: readability
severity: should_fix
---

**Rule.** In class and interface header comments containing usage examples,
organize examples logically by listing the simplest use cases first and grouping
related or alternative operating modes together. When adding new modes or
examples, place them alongside their related counterparts rather than appending
them after concluding summary paragraphs (such as sections introduced by
'Finally, ...').

**Why.** Organizing class documentation from simple to advanced with related
modes grouped together helps developers quickly discover appropriate usage
patterns and prevents misplaced additions from fragmenting structured
walkthroughs.

## Examples

Bad:

```cpp
// Finally, `TestFuture` also supports no-args callbacks:
//
//   Example for no-args callbacks:
//     TEST_F(MyTestFixture, MyTest) { ... }
//
//   Example for queueing values:
//     TEST_F(MyTestFixture, MyQueueTest) { ... }
```

Good:

```cpp
//   `TestFuture` can also operate in a queued mode by passing `Mode::Queue` to
//   the constructor.
//
//     TEST_F(MyTestFixture, MyQueueTest) { ... }
//
// Finally, `TestFuture` also supports no-args callbacks:
//
//   Example for no-args callbacks:
//     TEST_F(MyTestFixture, MyTest) { ... }
```
