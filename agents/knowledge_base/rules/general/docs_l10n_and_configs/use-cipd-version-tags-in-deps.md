---
name: general/use-cipd-version-tags-in-deps
rubric: readability
severity: nit
---

**Rule.** In DEPS files, reference CIPD package dependencies (especially 3pp
packages and multiplatform packages using `${{platform}}` or `${platform}`) by
their CIPD version tag (e.g. `version:3@<version>` or `version:2@<version>`)
rather than a raw instance ID hash.

**Why.** CIPD packages built via 3pp provide version tags that apply uniformly
across target architectures. Using version tags makes DEPS self-documenting and
human-readable, and is required for multiplatform packages parameterized with
`${{platform}}` so that gclient sync can resolve matching packages across
different host OS and CPU architectures, whereas raw instance hashes are
platform-specific archive digests.

## Examples

Bad:

```cpp
    'src/third_party/gperf/cipd': {
        'packages': [
          {
            'package': 'infra/3pp/tools/gperf/${{platform}}',
            'version': 'dMQ9whbP-g4zlZY2ggzFQ5dSusSx1fx7InTHnmpzSXgC', # v3.2
          },
        ],
        'condition': 'checkout_android or checkout_linux',
        'dep_type': 'cipd',
    },
```

Good:

```cpp
    'src/third_party/gperf/cipd': {
        'packages': [
          {
            'package': 'infra/3pp/tools/gperf/${{platform}}',
            'version': 'version:3@3.2',
          },
        ],
        'condition': 'host_os == "linux"',
        'dep_type': 'cipd',
    },
```
