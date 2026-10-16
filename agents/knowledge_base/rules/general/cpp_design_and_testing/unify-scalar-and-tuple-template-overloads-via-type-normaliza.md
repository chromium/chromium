---
name: general/unify-scalar-and-tuple-template-overloads-via-type-normaliza
rubric: design
severity: should_fix
---

**Rule.** When designing template functions or adapter classes that unpack
multi-value return types (such as std::tuple) into variadic argument packs or
callbacks, unify scalar and tuple handling into a single template overload using
a type-normalization helper trait (e.g. wrapping non-tuple types into a
single-element std::tuple<T>). Do not maintain disjoint template overloads
constrained by parameter pack sizes (such as requires(sizeof...(Types) > 1)).

**Why.** Maintaining separate overloads with pack-size constraints duplicates
template declarations across delegating classes (such as TaskRunner and
SequenceBound), introduces edge-case failure on single-element tuples, and
complicates overload resolution. Normalizing types to tuples and unpacking
uniformly with std::apply handles 0, 1, and N arguments cleanly in a single
implementation.

## Examples

Bad:

```cpp
// Bad: Disjoint overloads with pack size constraints
template <typename TaskReturnType, typename ReplyArgType>
bool PostTaskAndReplyWithResult(TaskCallbackType<TaskReturnType()> task,
                                ReplyCallbackType<void(ReplyArgType)> reply);

template <typename... TupleTypes, typename... ReplyArgTypes>
  requires(sizeof...(TupleTypes) > 1 && sizeof...(ReplyArgTypes) > 1)
bool PostTaskAndReplyWithResult(
    TaskCallbackType<std::tuple<TupleTypes...>()> task,
    ReplyCallbackType<void(ReplyArgTypes...)> reply);
```

Good:

```cpp
// Good: Unified overload using a helper trait to normalize scalar and tuple types
template <typename TaskReturnType, typename... ReplyArgTypes>
bool PostTaskAndReplyWithResult(
    TaskCallbackType<TaskReturnType()> task,
    ReplyCallbackType<void(ReplyArgTypes...)> reply) {
  using ReplyStorageType =
      typename internal::ensure_tuple<TaskReturnType>::type;
  ...
}
```
