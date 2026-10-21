---
name: chrome/use-dot-separator-in-profile-version-filenames
rubric: readability
severity: nit
---

**Rule.** Format build profile, PGO, and orderfile version tracking filenames
using a dot separator between the target platform/architecture and artifact kind
(e.g., `<target>.<kind>.txt` such as `android-arm64.orderfile.txt` or
`win64.pgo.txt`) rather than hyphens. Follow this convention across file
declarations, DEPS entries, GN inputs, and OWNERS per-file directives.

**Why.** Using `<target>.<kind>.txt` conforms to established Chromium profile
tracking conventions (e.g. `android-arm64.pgo.txt`), standardizes file naming
across platforms, and enables wildcard glob matching (such as
`per-file *.orderfile.txt`) in OWNERS files and autoroller configurations.

## Examples

Bad:

```cpp
# Hyphenated artifact name prevents glob matching
'version_file': 'src/chrome/build/android-arm64-orderfile.txt'
inputs += [ "//chrome/build/android-arm64-orderfile.txt" ]
```

Good:

```cpp
# Dot-separated <target>.<kind>.txt enables per-file *.orderfile.txt rules
'version_file': 'src/chrome/build/android-arm64.orderfile.txt'
inputs += [ "//chrome/build/android-arm64.orderfile.txt" ]
```
