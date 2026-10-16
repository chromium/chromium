---
name: general/avoid-multi-clause-python-comprehensions
rubric: readability
severity: should_fix
---

**Rule.** Do not use list comprehensions, dict/set comprehensions, or generator
expressions with multiple 'for' clauses or complex nested iterations. Write
explicit standard 'for' loops (using methods such as list.append() or
list.extend()) instead.

**Why.** Per Google's Python Style Guide, comprehensions are intended for simple
transformations where each clause fits on one line. Multiple 'for' clauses and
nested iterations in comprehensions harm readability and make complex iteration
logic hard to debug.

## Examples

Bad:

```cpp
fuchsia_buildflags = [
    result for f in input_api.AffectedSourceFiles(SourceFilter)
    for result in _CheckNoIsFuchsiaBuildFlagsInChromeFile(input_api, f)
]
```

Good:

```cpp
fuchsia_buildflags = []
for f in input_api.AffectedSourceFiles(SourceFilter):
    fuchsia_buildflags.extend(
        _CheckNoIsFuchsiaBuildFlagsInChromeFile(input_api, f))
```
