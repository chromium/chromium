---
name: third_party/use-copybara-strictly-for-transformations-in-crowbar
rubric: design
severity: nit
---

**Rule.** In Crowbar third-party dependency workflows, use Copybara
(`copy.bara.sky`) strictly as a code transformation tool; do not configure
Copybara origin fetch operations to retrieve upstream source code directly.
Specify upstream fetch sources, URLs, and revisions in `crowbar.txt` or
`crowbar.txtpb`, and restrict Copybara transformations to moving, pruning, or
patching files.

**Why.** Crowbar centralizes source code fetching across third-party
dependencies to ensure reproducibility, auditability, and consistent upstream
tracking across local workstations and automated CI rollers. Configuring
Copybara to fetch upstream sources directly bypasses Crowbar's fetch management
and causes configuration drift.

## Examples

Bad:

```cpp
# In third_party/<dep>/copy.bara.sky
core.workflow(
    name = "default",
    origin = git.origin(
        url = "https://github.com/upstream/repo.git",
        ref = "main",
    ),
    destination = git.destination(url = "...", fetch = "..."),
)
```

Good:

```cpp
# In third_party/<dep>/crowbar.txtpb (or crowbar.txt)
# Fetch configuration is handled by Crowbar:
# git_source { url: "https://github.com/upstream/repo.git" ref: "v1.0.0" }

# In third_party/<dep>/copy.bara.sky
load("//crowbar", "crowbar_workflow")

crowbar_workflow(
    transformations = [
        core.move("src", ""),
        patch.apply(patches = ["patches/0001-fix.patch"]),
    ],
)
```
