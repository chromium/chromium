---
name: general/consistent-ts-private-protected-member-naming
rubric: readability
severity: optional
---

**Rule.** In TypeScript code and documentation, do not mix and match naming
styles for `private` and `protected` members within a single file. While
Chromium explicitly permits trailing underscores as an exception to the Google
TypeScript Style Guide, each file must consistently either use trailing
underscores (e.g., `foo_`) or omit them (e.g., `foo`) across all its private and
protected members.

**Why.** Chromium permits trailing underscores on private and protected
TypeScript members to minimize churn when migrating legacy JavaScript and
Polymer code. However, mixing trailing-underscore and non-underscore naming
styles within the same file creates visual inconsistency and impairs
readability.

## Examples

Bad:

```cpp
class MyElement extends CrLitElement {
  private foo_: string = '';
  private bar: number = 0; // Inconsistent: mixes trailing underscore with non-underscore

  protected onClick_() {}
  protected onHover() {} // Inconsistent: mixes method naming styles
}
```

Good:

```cpp
class MyElement extends CrLitElement {
  // Consistently using trailing underscores:
  private foo_: string = '';
  private bar_: number = 0;

  protected onClick_() {}
  protected onHover_() {}
}
```
