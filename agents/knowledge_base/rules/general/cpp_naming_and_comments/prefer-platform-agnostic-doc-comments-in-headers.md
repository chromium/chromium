---
name: general/prefer-platform-agnostic-doc-comments-in-headers
rubric: readability
severity: should_fix
---

**Rule.** In cross-platform API headers, describe behavior and preconditions
using concise, platform-agnostic concepts rather than fragmented
platform-by-platform breakdowns when the overall contract is conceptually
uniform across platforms. Explicitly define link and reparse point
semantics—clarifying whether symlinks and Windows reparse points (such as
directory junctions) are traversed or deleted directly—and avoid inaccurate
generalizations about mount points that ignore POSIX behavior.

**Why.** Fragmented platform callouts clutter public headers when semantics are
conceptually uniform, while overly vague generalizations (such as conflating
POSIX mount points with Windows directory junctions) obscure whether links are
followed, skipped, or deleted. Specifying precise link and reparse point
handling ensures callers understand filesystem operations without unnecessary
platform boilerplate.

## Examples

Bad:

```cpp
// On Windows, it does not follow directory junctions (reparse points) or
// symlinks.
//
// In POSIX environment and if |path| is a symbolic link, this deletes only
// the symlink. (even if the symlink points to a non-existent file)
BASE_EXPORT bool DeletePathRecursively(const FilePath& path);

// It doesn't follow symlinks or mount points.
BASE_EXPORT bool DeleteFile(const FilePath& path);
```

Good:

```cpp
// Deletes the given path, whether it's a file or a directory.
// It does not traverse symlinks or Windows reparse points (e.g., directory
// junctions), but instead deletes the link or reparse point itself.
BASE_EXPORT bool DeletePathRecursively(const FilePath& path);
```
