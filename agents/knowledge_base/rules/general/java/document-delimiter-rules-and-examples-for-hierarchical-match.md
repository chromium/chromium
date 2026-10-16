---
name: general/document-delimiter-rules-and-examples-for-hierarchical-match
rubric: readability
severity: nit
---

**Rule.** When documenting API functions that match, filter, or group structured
or hierarchical identifiers (such as bundle IDs, domain names, package names,
URLs, or file paths) by prefix, explicitly specify the delimiter and hierarchy
requirements (e.g., dot-delimited component matching vs. raw substring matching)
rather than using ambiguous phrasing like 'common prefix'. Include concrete
positive and negative examples showing which identifier variations match or fail
to match.

**Why.** Vague terms like 'common prefix' leave callers and maintainers
uncertain whether matching is performed at the raw string level or respects
component delimiter boundaries (e.g., whether 'com.example.App' matches
'com.example.App-Foo' vs. 'com.example.App.Helper'). Specifying delimiter rules
and providing explicit positive and negative examples makes API matching
semantics unambiguous and prevents subtle process-grouping, domain-matching, or
path-filtering bugs.

## Examples

Bad:

```cpp
// This function identifies the top-most process in the tree that still
// shares a common bundle identity prefix. This allows callers to group
// helper processes with the main application.
std::optional<std::string> GetMainBundleIdForProcess(pid_t pid);
```

Good:

```cpp
// This function identifies the top-most ancestor in the process tree where
// the ancestor's bundle ID is a dot-delimited prefix of the child's
// bundle ID.
//
// For example:
// - "com.example.App" is a prefix of "com.example.App.Helper".
// - "com.example.App-Foo" is NOT a prefix of "com.example.App.Helper" because
//   it does not follow the dot-delimited hierarchy.
//
// This allows callers to attribute helper processes back to the
// main application.
std::optional<std::string> GetMainBundleIdForProcess(pid_t pid);
```
