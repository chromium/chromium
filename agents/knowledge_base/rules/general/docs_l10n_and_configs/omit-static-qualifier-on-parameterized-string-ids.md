---
name: general/omit-static-qualifier-on-parameterized-string-ids
rubric: readability
severity: should_fix
---

**Rule.** In GRD and GRDP resource files (.grd, .grdp), do not retain '_STATIC'
or static qualifiers in <message> resource IDs (IDS_\*) when the string contains
dynamic replacement placeholders (<ph>). Ensure message identifiers accurately
describe the string's semantic role and maintain naming consistency across
parallel actions.

**Why.** Qualifiers like '\_STATIC' in a string resource ID signify fixed text
without runtime argument substitution. Retaining '\_STATIC' on a string
parameterized with dynamic placeholders is misleading to callers and reviewers
who rely on the identifier to know whether argument formatting is expected.

## Examples

Bad:

```cpp
<message name="IDS_FEDERATION_TITLE_STATIC" desc="Title of the UI shown in the omnibox to remind users that they can sign in with an identity provider.">
  Sign in with <ph name="IDP_NAME">$1<ex>google.com</ex></ph>
</message>
```

Good:

```cpp
<message name="IDS_FEDERATION_SIGN_IN_TITLE" desc="Title of the UI shown in the omnibox to remind users that they can sign in with an identity provider.">
  Sign in with <ph name="IDP_NAME">$1<ex>google.com</ex></ph>
</message>
```
