---
name: general/match-roll-log-url-to-rolled-revisions
rubric: readability
severity: should_fix
---

**Rule.** When authoring commit messages for dependency rolls or updates
spanning upstream revisions, include a link to the upstream git log or changelog
(e.g. Gitiles `+log/<start>..<end>` or GitHub comparison link) in the commit
message body. Ensure that the comparison URL matches the exact starting and
ending commit hashes or tags specified in `DEPS`, Git submodules, and the CL
subject line. Additionally, when linking to Gitiles log comparisons for mirrored
third-party repositories (`chromium.googlesource.com/external/...`), prefix tag
or branch names with `upstream/` (e.g., `+log/upstream/v1.0..upstream/v1.1`) so
URLs resolve correctly.

**Why.** Reviewers and post-commit auditors investigating regressions, security
fixes, or bisections require immediate visibility into what changed upstream
across the rolled revision range. Omitting the log link requires manual git
archaeology, mismatched commit hashes present inaccurate diff ranges, and
omitting the `upstream/` namespace prefix on mirrored third-party tags causes
Gitiles links to fail to resolve.

## Examples

Bad:

```cpp
# Bad: Missing upstream git log comparison URL.
Roll Truth from 33387149b465 to fc54b9e4436e.

Roll includes 435 revisions.
Full hash: fc54b9e4436ed8ca8bcae7dd71e07a6857e13836

# Bad: Log comparison URL starting hash does not match rolled revisions.
Roll libc++ from cb9c06560a68 to af4386908c37 (64 revisions)

https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libcxx.git/+log/7ab65651aed6..af4386908c37

# Bad: Mirrored third-party Gitiles log link omits upstream/ prefix on tags.
Roll cpu_features from v0.8.0 to v0.10.1 (3 revisions)

https://chromium.googlesource.com/external/github.com/google/cpu_features/+log/v0.8.0..v0.10.1
```

Good:

```cpp
# Good: Includes git log comparison URL matching exact rolled revision range.
Roll Truth from 33387149b465 to fc54b9e4436e.

Roll includes 435 revisions.
Full hash: fc54b9e4436ed8ca8bcae7dd71e07a6857e13836

Log of Truth changes:
https://chromium.googlesource.com/external/github.com/google/truth.git/+log/33387149b465..fc54b9e4436e

# Good: Log URL matches the exact starting commit SHA.
Roll libc++ from cb9c06560a68 to af4386908c37 (64 revisions)

https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libcxx.git/+log/cb9c06560a68..af4386908c37

# Good: Mirrored third-party Gitiles log comparison uses upstream/ prefix on tags.
Roll cpu_features from v0.8.0 to v0.10.1 (3 revisions)

https://chromium.googlesource.com/external/github.com/google/cpu_features/+log/upstream/v0.8.0..upstream/v0.10.1
```
