---
name: geic/keep-mojom-enums-and-geic-histograms-synced
rubric: correctness
severity: should_fix
---

**Rule.** In `gemini_enterprise.mojom`, mark enums used in IPC responses or
metrics (such as `OpenAuthTabResult`) as `[Extensible]` with a
`[Default] kUnknown = 0` enumerator, wrap Mojo method responses in structs (such
as `OpenAuthTabResponse`) for backwards compatibility, pair Mojo/C++ enums with
`// LINT.IfChange(...)` and `// LINT.ThenChange(...)` pointing to
`//tools/metrics/histograms/metadata/glic/enums.xml`, and name GEiC histograms
with the `Geic.` prefix (never `Glic.Geic.`).

**Why.** The GEiC WebUI client and browser host evolve across versions;
`[Extensible]` enums with `[Default] kUnknown = 0` and response structs allow
adding status codes or fields without breaking older clients. `LINT.IfChange`
prevents silent histogram bucket drift, and the top-level `Geic.` prefix matches
the histogram metadata registration in
`tools/metrics/histograms/metadata/glic/histograms.xml`.

## Examples

Bad:

```mojom
enum OpenAuthTabResult {
  kSuccess = 0,
  kErrorDisallowedUrl = 1,
};

interface GeminiEnterpriseHandler {
  OpenAuthTab(OpenAuthTabOptions options) => (OpenAuthTabResult result);
};
```

Good:

```mojom
// LINT.IfChange(OpenAuthTabResult)
[Extensible]
enum OpenAuthTabResult {
  [Default] kUnknown = 0,
  kSuccess = 1,
  kErrorNoUrl = 2,
  kErrorDisallowedUrl = 3,
  kErrorFailure = 4,
  kErrorInvalidPurpose = 5,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GeicOpenAuthTabResult)

struct OpenAuthTabResponse {
  OpenAuthTabResult result;
};

interface GeminiEnterpriseHandler {
  OpenAuthTab(OpenAuthTabOptions options) => (OpenAuthTabResponse response);
};
```
