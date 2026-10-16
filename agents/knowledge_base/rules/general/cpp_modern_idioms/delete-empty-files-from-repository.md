---
name: general/delete-empty-files-from-repository
rubric: readability
severity: blocking
---

**Rule.** When removing all declarations, definitions, or content from a file
during refactoring or cleanup, remove the file completely from version control
(git rm) rather than leaving an empty zero-byte file tracked in the repository.
Also clean up any build targets or header inclusions referencing the removed
file.

**Why.** Leaving zero-byte empty files tracked in the repository clutters the
tree, leaves orphaned entries in build files and code navigation tools, and is
often an unintended artifact of automated refactoring scripts.

## Examples

Bad:

```cpp
// All contents removed during refactoring, but empty file remains tracked in git:
// diff --git a/base/strings/latin1_string_conversions.h b/base/strings/latin1_string_conversions.h
// index 384707da38b73..e69de29bb2d1d 100644
```

Good:

```cpp
# Remove the obsolete file completely from version control
git rm base/strings/latin1_string_conversions.h
```
