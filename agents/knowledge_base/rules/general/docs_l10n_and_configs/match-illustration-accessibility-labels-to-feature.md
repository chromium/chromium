---
name: general/match-illustration-accessibility-labels-to-feature
rubric: testing
severity: should_fix
---

**Rule.** When defining localized accessibility labels or alt text
(\_A11Y_LABEL, \_ALT_TEXT, \_ARIA_LABEL) for illustrations, preview images, or
UI elements in .grd and .grdp files, ensure the translatable message text
accurately describes the specific feature and graphic. Do not retain copy-pasted
message text from another feature or screen.

**Why.** Screen readers announce illustration accessibility labels and alt text
to convey visual information to users with visual impairments. Retaining
copy-pasted string content between feature definitions causes screen readers to
announce incorrect, misleading feature descriptions, degrading accessibility.

## Examples

Bad:

```cpp
<!-- Copy-pasted string text from Google Lens into Password Manager showcase -->
<message name="IDS_FEATURE_SHOWCASE_PASSWORD_MANAGER_ILLUSTRATION_A11Y_LABEL" desc="Accessibility label for the illustration on the Google Password Manager page in the Feature Showcase.">
  A simplified image showing how you can use Google Lens to look up a product
</message>
```

Good:

```cpp
<!-- Accurately describes the Password Manager illustration -->
<message name="IDS_FEATURE_SHOWCASE_PASSWORD_MANAGER_ILLUSTRATION_A11Y_LABEL" desc="Accessibility label for the illustration on the Google Password Manager page in the Feature Showcase.">
  A simplified image showing the Google Password Manager entry point in Chrome
</message>
```
