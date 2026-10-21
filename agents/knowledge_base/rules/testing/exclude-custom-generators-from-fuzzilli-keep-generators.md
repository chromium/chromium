---
name: testing/exclude-custom-generators-from-fuzzilli-keep-generators
rubric: readability
severity: should_fix
---

**Rule.** In Fuzzilli profile definitions, do not include custom profile
generator names in the keepGenerators allowlist used to filter CodeGenerators.
CodeGenerators only contains Fuzzilli's default built-in generators, whereas
custom profile generators are registered separately via additionalCodeGenerators
and are never disabled by filtering CodeGenerators.

**Why.** CodeGenerators contains only Fuzzilli's built-in generator instances.
Adding custom generator names to keepGenerators has no effect because those
custom generators are not present in CodeGenerators, creating redundant code and
reflecting a misconception of how custom generators are registered and disabled.

## Examples

Bad:

```cpp
private let keepGenerators = [
    "IntegerGenerator", "StringGenerator", "TernaryOperationGenerator",
    "PlainFunctionGenerator", "AsyncFunctionGenerator", "AwaitGenerator",
    "SubroutineReturnGenerator", "TryCatchGenerator",
    "MojoRouterListenerGenerator", "MojoBuiltinGenerator",
]

private let mojoDisabledGenerators: [String] = CodeGenerators.map { $0.name }
    .filter {
        !keepGenerators.contains($0)
    }
```

Good:

```cpp
private let keepGenerators = [
    "IntegerGenerator", "StringGenerator", "TernaryOperationGenerator",
    "PlainFunctionGenerator", "AsyncFunctionGenerator", "AwaitGenerator",
    "SubroutineReturnGenerator", "TryCatchGenerator",
]

private let mojoDisabledGenerators: [String] = CodeGenerators.map { $0.name }
    .filter {
        !keepGenerators.contains($0)
    }
```
