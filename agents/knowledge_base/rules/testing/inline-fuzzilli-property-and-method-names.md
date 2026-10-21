---
name: testing/inline-fuzzilli-property-and-method-names
rubric: design
severity: should_fix
---

**Rule.** In Fuzzilli profile, ILType, and ObjectGroup definitions, write
property and method names directly as inline string literals (such as
`withProperties: ["$interfaceName"]` or `withMethods: ["getRemote"]`) rather
than declaring dedicated constant enums or static properties for short member
names. Reserve shared constant namespaces (such as `MojoStrings` or
`CommonMojoStrings`) for long or fully qualified interface and object group
identifiers.

**Why.** Interface and object group names are long, fully qualified identifiers
reused across types, builtins, and generators where constants prevent typos. In
contrast, property and method names are short, local member keys; creating
dedicated constant enums for each property and method adds indirection and
boilerplate without providing meaningful safety or reuse.

## Examples

Bad:

```cpp
private enum MojoProperties {
  static let interfaceName = "$interfaceName"
}
private enum MojoMethods {
  static let getRemote = "getRemote"
}

fileprivate static let jsLockManager: ILType = .object(
  ofGroup: MojoStrings.lockManager,
  withProperties: [MojoProperties.interfaceName],
  withMethods: [MojoMethods.getRemote])
```

Good:

```cpp
fileprivate static let jsLockManager: ILType = .object(
  ofGroup: MojoStrings.lockManager,
  withProperties: ["$interfaceName"],
  withMethods: ["getRemote"])
```
