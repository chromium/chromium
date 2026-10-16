---
name: general/document-non-trivial-apis-with-usage-examples
rubric: readability
severity: nit
---

**Rule.** When declaring non-trivial APIs, functions, or variadic/templated
constructors in header files (especially those accepting parameter packs,
concept constraints, or optional traits alongside required parameters), include
concrete call-site examples in their declaration doc comments. Show realistic
invocations illustrating both minimal required configurations and calls
combining required with optional arguments.

**Why.** Complex signatures with variadic templates, trailing parameter packs,
or optional trait lists leave the expected call-site syntax ambiguous (such as
whether arguments must be wrapped in nested structs, initializer lists, or
passed flat). Concrete call-site examples in headers clarify consumption
patterns, prevent API misuse, and save callers from having to decipher template
deduction constraints.

## Examples

Bad:

```cpp
// Constructs a MemoryConsumerTraits with the specified required traits and
// zero or more optional traits. Uses the default value for optional traits
// that are not specified.
template <typename... Args>
  requires trait_helpers::AreValidTraits<OptionalTraitsList, Args...>
constexpr MemoryConsumerTraits(EstimatedMemoryUsage estimated_memory_usage,
                               ReleaseMemoryCost release_memory_cost,
                               InformationRetention information_retention,
                               ExecutionType execution_type,
                               Args... args)
```

Good:

```cpp
// Constructs a MemoryConsumerTraits with the specified required traits and
// zero or more optional traits. Uses the default value for optional traits
// that are not specified.
//
// Examples:
//   // Only required traits:
//   constexpr base::MemoryConsumerTraits traits(
//       base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
//       base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
//       base::MemoryConsumerTraits::InformationRetention::kLossy,
//       base::MemoryConsumerTraits::ExecutionType::kSynchronous);
//
//   // Required + some optional traits:
//   constexpr base::MemoryConsumerTraits traits(
//       base::MemoryConsumerTraits::EstimatedMemoryUsage::kMedium,
//       base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
//       base::MemoryConsumerTraits::InformationRetention::kLossless,
//       base::MemoryConsumerTraits::ExecutionType::kAsynchronous,
//       base::MemoryConsumerTraits::InProcess::kNo,
//       base::MemoryConsumerTraits::IsStateful::kNo);
//
template <typename... Args>
  requires trait_helpers::AreValidTraits<OptionalTraitsList, Args...>
constexpr MemoryConsumerTraits(EstimatedMemoryUsage estimated_memory_usage,
                               ReleaseMemoryCost release_memory_cost,
                               InformationRetention information_retention,
                               ExecutionType execution_type,
                               Args... args)
```
