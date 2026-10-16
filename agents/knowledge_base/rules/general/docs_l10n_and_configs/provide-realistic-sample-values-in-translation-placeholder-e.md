---
name: general/provide-realistic-sample-values-in-translation-placeholder-e
rubric: testing
severity: nit
---

**Rule.** In GRD and GRDP string resource files (.grd, .grdp), provide
realistic, representative sample values inside placeholder <ex> tags (such as
'example.com' for URLs or 'johndoe@example.com' for emails) rather than
repeating placeholder identifiers or generic types (like 'url' or 'string').
When providing sample application or service names, use generic, role-based
descriptors (such as 'Calculator App' or 'Parent app') rather than hardcoding
specific third-party commercial brand names or trademarks (such as 'Citrix').

**Why.** Translators rely on <ex> tags inside <ph> placeholders to understand
the runtime data format, part of speech, and grammatical context of substituted
text. Repeating placeholder type names provides insufficient context and leads
to mistranslations, while hardcoding third-party commercial brand names into
localization assets is unnecessary and improper for open-source Chromium
resources.

## Examples

Bad:

```cpp
<!-- Bad: Repeating placeholder identifier -->
<message name="IDS_PASSWORD_MANAGER_MOVE_PASSWORDS_DIALOG" desc="...">
  Passwords for "<ph name="URL">$1<ex>url</ex></ph>" are saved locally.
</message>
<!-- Bad: Using third-party commercial brand name -->
<message name="IDS_APP_MANAGEMENT_IS_SUB_APP_PERMISSION" desc="...">
  Permissions allowed for <ph name="PARENT_APP_NAME">$2<ex>Citrix</ex></ph> and its apps.
</message>
```

Good:

```cpp
<!-- Good: Realistic sample domain -->
<message name="IDS_PASSWORD_MANAGER_MOVE_PASSWORDS_DIALOG" desc="...">
  Passwords for "<ph name="URL">$1<ex>example.com</ex></ph>" are saved locally.
</message>
<!-- Good: Generic, role-based app descriptor -->
<message name="IDS_APP_MANAGEMENT_IS_SUB_APP_PERMISSION" desc="...">
  Permissions allowed for <ph name="PARENT_APP_NAME">$2<ex>Parent app</ex></ph> and its apps.
</message>
```
