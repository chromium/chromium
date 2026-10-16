---
name: general/avoid-personal-pronouns-in-comments
rubric: readability
severity: should_fix
---

**Rule.** Do not use conversational personal pronouns (such as `you`, `your`,
`we`, or `our`) in code comments and technical documentation. Phrase comments
directly and concisely using the imperative mood for instructions (e.g.,
`Use Foo() to...`) or third-person declarative statements for behavioral
descriptions.

**Why.** Conversational pronouns add unnecessary wordiness and shift the focus
from the code's behavior to the reader or author. Direct imperative or
descriptive phrasing makes comments more concise, professional, and easier to
scan.

## Examples

Bad:

```cpp
// If you want to adopt a handle of a particular type, you can use
// TakeHandleOfType() for additional validation.
// We also need to check whether the handle is valid before proceeding.
```

Good:

```cpp
// Use TakeHandleOfType() to adopt a handle of a particular type with
// additional validation.
// Ensure the handle is valid before proceeding.
```
