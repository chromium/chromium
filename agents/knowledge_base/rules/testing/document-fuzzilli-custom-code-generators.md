---
name: testing/document-fuzzilli-custom-code-generators
rubric: readability
severity: optional
---

**Rule.** When defining custom `CodeGenerator` instances in Fuzzilli fuzzing
profiles, precede each generator with a doc comment explaining its purpose, what
program constructs or types it emits, and the rationale for requiring a
dedicated generator instead of relying on generic generators (such as increasing
the generation frequency of target objects or handling multi-step interface
setup).

**Why.** Fuzzilli profiles define custom code generators to steer generation
toward specific interfaces or handle multi-step initialization. Without
explanatory comments, maintainers and reviewers cannot easily determine what
constructs a generator emits or why a dedicated generator is necessary instead
of relying on generic generation strategies.

## Examples

Bad:

```cpp
private let MojoLockManagerRemoteGenerator = CodeGenerator(
  "MojoLockManagerRemoteGenerator", produces: [.jsLockManagerRemote]
) { b in
  let managerStatic = b.createNamedVariable(
    forBuiltin: "blink.mojom.LockManager")
  b.callMethod("getRemote", on: managerStatic, withArgs: [])
}
```

Good:

```cpp
/// Creates a LockManager variable and calls getRemote on it to get a
/// LockManagerRemote for use by other CodeGenerators. Although
/// MojoBuiltinGenerator and MojoMethodCallGenerator can together generate the
/// same code, this dedicated generator increases the frequency of
/// LockManagerRemote objects in programs.
private let MojoLockManagerRemoteGenerator = CodeGenerator(
  "MojoLockManagerRemoteGenerator", produces: [.jsLockManagerRemote]
) { b in
  let managerStatic = b.createNamedVariable(
    forBuiltin: "blink.mojom.LockManager")
  b.callMethod("getRemote", on: managerStatic, withArgs: [])
}
```
