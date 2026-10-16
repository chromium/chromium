---
name: general/when-checking-for-identifiers-or-parameter-names-in-source-t
rubric: correctness
severity: should_fix
---

**Rule.** When checking whether an identifier, token, or parameter name appears
in structured text (such as source code lines, JSON, or diff contents) in
presubmit or tooling scripts, do not use unconstrained substring checks (e.g.,
`if token in text`). Match using exact string delimiters (such as enclosing
quotes for JSON or string literals), regex word boundaries (`\b`), or structured
parsing to avoid false-positive matches on identifiers that share substrings.

**Why.** Raw substring checks (`token in text`) match whenever a short
identifier is a substring or prefix of an unrelated identifier (such as
`MyParam` inside `MyParamLong` or `kFeature` inside `kFeatureEnabled`), causing
false-positive presubmit warnings or misattributed tooling flags. Enclosing
tokens in delimiters or matching on word boundaries guarantees exact identifier
matching.

## Examples

Bad:

```cpp
# Unconstrained substring check matches if 'param' is a prefix/substring of another parameter.
for param in params:
  if param in changed_contents:
    probably_affected = True
    break
```

Good:

```cpp
# Match with enclosing quotes or word boundaries to prevent substring false positives.
for param in params:
  if f'"{param}"' in changed_contents:
    probably_affected = True
    break
```
