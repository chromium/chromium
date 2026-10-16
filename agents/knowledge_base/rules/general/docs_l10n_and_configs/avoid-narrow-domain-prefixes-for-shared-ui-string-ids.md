---
name: general/avoid-narrow-domain-prefixes-for-shared-ui-string-ids
rubric: readability
severity: should_fix
---

**Rule.** In `.grd` and `.grdp` resource files, do not use narrow, domain- or
account-type-specific prefixes (such as `ENTERPRISE_`, `SUPERVISED_`, or
`MANAGED_`) in `<message>` resource identifiers (`IDS_*`) when the string is
displayed in or shared across general or multi-purpose user flows. Prefix string
IDs with the overarching UI feature, screen, or surface name (such as `FRE_`,
`SIGNIN_`, or `PROFILES_`) instead.

**Why.** Prefixing string resource identifiers with a specialized domain or
account category misleads developers and translators into assuming the string is
exclusively used for that sub-flow, causing confusion when the string is
presented to general or consumer users. Naming IDs after the shared UI surface
reflects where the text appears and preserves clean semantics when flows evolve.

## Examples

Bad:

```cpp
<!-- In chrome/app/profiles_strings.grdp -->
<message name="IDS_ENTERPRISE_WELCOME_PROFILE_DISCLOSURE_TITLE_REFRESH" desc="Title for the profile welcome disclosure screen in first run.">
  Welcome, <ph name="USER_NAME">$1<ex>John</ex></ph>!
</message>
```

Good:

```cpp
<!-- In chrome/app/profiles_strings.grdp -->
<message name="IDS_FRE_SIGN_IN_CELEBRATION_WELCOME_TITLE" desc="Title for the sign-in celebration screen in first run experience (FRE).">
  Welcome, <ph name="USER_NAME">$1<ex>John</ex></ph>!
</message>
```
