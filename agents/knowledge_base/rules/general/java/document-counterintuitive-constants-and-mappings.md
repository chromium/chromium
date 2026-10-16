---
name: general/document-counterintuitive-constants-and-mappings
rubric: readability
severity: nit
---

**Rule.** When mapping, branching on, or checking constants, enums, class names,
or string identifiers with counter-intuitive values, tool/parser quirks (such as
unexpected casing requirements), or non-exact prefix matching where exact
equality would normally be expected (such as dynamic numeric suffixes), add an
inline explanatory comment directly at the code site. Do not rely solely on
commit messages to explain intentional quirks or non-obvious matching
constraints.

**Why.** External APIs, OS platforms, parsers, and dynamic naming conventions
often have quirks or require pattern matching rather than exact equality.
Explaining the constraint directly in an inline comment ensures future
maintainers understand the rationale and prevents them from mistaking
intentional quirks or prefix checks for bugs or over-simplifying them.

## Examples

Bad:

```cpp
// Variant 1: C++ counter-intuitive constant classification
switch (product_type) {
  case PRODUCT_IOTENTERPRISES:
  case PRODUCT_SERVERRDSH:
    version_type_ = SUITE_ENTERPRISE;
    break;
}

# Variant 2: Python external tool/parser quirk without inline explanation
git_footers = input_api.change.GitFootersFromDescription()
if 'true' in [footer.lower() for footer in git_footers.get(
        u'Allow-Mainlayoutswitcher-Changes', [])]:
    return []

// Variant 3: Java prefix match on class name without explaining dynamic suffix
String className = mFallbackServiceName.getClassName();
if (className.startsWith("org.chromium.content.app.SandboxedProcessService")) {
    serviceName = ServiceNames.JAVA_SANDBOXED;
}
```

Good:

```cpp
// Variant 1: Inline comment explaining external constant mapping
switch (product_type) {
  case PRODUCT_IOTENTERPRISES:
  // PRODUCT_SERVERRDSH (0xAF) is reused by Windows for Enterprise Multi-Session.
  case PRODUCT_SERVERRDSH:
    version_type_ = SUITE_ENTERPRISE;
    break;
}

# Variant 2: Inline comment explaining parser title-casing quirk
# git_footers.py splits keys on '-' and title-cases each word, so
# 'Mainlayoutswitcher' must have lowercase 'l' and 's' to match.
git_footers = input_api.change.GitFootersFromDescription()
if 'true' in [footer.lower() for footer in git_footers.get(
        u'Allow-Mainlayoutswitcher-Changes', [])]:
    return []

// Variant 3: Inline comment explaining why prefix matching is required
String className = mFallbackServiceName.getClassName();
// Don't use exact match because service names have a numeric suffix.
if (className.startsWith("org.chromium.content.app.SandboxedProcessService")) {
    serviceName = ServiceNames.JAVA_SANDBOXED;
}
```
