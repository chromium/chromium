# Lint Sync Patterns & Syntax Rules

The linter links files by cross-referencing labels. Follow these syntax rules to
format your `LINT.IfChange` and `LINT.ThenChange` guards correctly.

## Syntax Rules

- `LINT.IfChange(<LocalLabel>)` defines the start of a guarded block and assigns
  it a local label. **Always use the name of the enum as it appears in the
  current file.**
- `LINT.ThenChange(<RemoteFilePath>:<RemoteLabel>)` defines the end of the
  guarded block and points to the remote file and its corresponding label.
  **Always use the name of the enum as it appears in the remote file.**

## Example where names do NOT match

- **Source enum:** `SecurityDomainId`
- **XML enum:** `TrustedVaultSecurityDomainId`

### C++/Java (Source Code)

```cpp
// LINT.IfChange(SecurityDomainId)
enum class SecurityDomainId { ... };
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:TrustedVaultSecurityDomainId)
```

### XML (`enums.xml` or `histograms.xml`)

```xml
<!-- LINT.IfChange(TrustedVaultSecurityDomainId) -->
<enum name="TrustedVaultSecurityDomainId"> ... </enum>
<!-- LINT.ThenChange(//path/to/source.h:SecurityDomainId) -->
```
