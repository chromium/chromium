---
name: base/document-trace-categories-with-set-description
rubric: readability
severity: should_fix
---

**Rule.** When defining or registering a new trace category in
`base/trace_event/builtin_categories.h`, insert the category in alphabetical
order by name within `PERFETTO_DEFINE_CATEGORIES_IN_NAMESPACE_WITH_ATTRS` and
use `.SetDescription(...)` to describe its purpose and expand subsystem
acronyms. Additionally, attach relevant tags (such as `.SetTags("debug")` for
debug categories) and identify owners in comments.

**Why.** Categories in `builtin_categories.h` are wrapped in
`// clang-format off`, so alphabetical ordering must be maintained manually to
ensure consistent lookup and avoid merge conflicts. Documenting categories with
`.SetDescription()` and tags ensures developers understand what events are
captured when configuring trace recordings and analyzing traces in Perfetto.

## Examples

Bad:

```cpp
// Out of alphabetical order and missing category descriptions:
perfetto::Category("content"),
perfetto::Category("content.fedcm"),
perfetto::Category("content.digitalcredentials"),
perfetto::Category("glic"),
```

Good:

```cpp
// Sorted alphabetically and documented with .SetDescription():
perfetto::Category("content"),
perfetto::Category("content.digitalcredentials").SetDescription(
    "Traces for the Digital Credentials API"),
perfetto::Category("content.fedcm").SetDescription(
    "Traces for the Federated Credential Management API"),
perfetto::Category("glic").SetDescription(
    "Events related to GLiC (Gemini Live in Chrome)."),
```
