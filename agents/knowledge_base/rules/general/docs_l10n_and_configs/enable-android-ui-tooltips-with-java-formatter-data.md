---
name: general/enable-android-ui-tooltips-with-java-formatter-data
rubric: testing
severity: should_fix
---

**Rule.** When building or updating Android UI components (such as buttons or
icon controls) that correspond to desktop views with tooltips, configure
tooltips on interactive controls using TooltipCompat.setTooltipText() (or
ViewCompat.setTooltipText()). Ensure the corresponding localized string resource
in .grd or .grdp files is annotated with formatter_data="android_java" so GRIT
generates the R.string identifier for Android Java callers.

**Why.** Tooltips provide essential accessibility context and hover hints for
mouse and stylus input on large screens, tablets, and Desktop Android. Strings
defined in shared .grd or .grdp resource files are excluded from Android Java
R.string generation by default to optimize APK size, requiring
formatter_data="android_java" to make them accessible in Java UI code.

## Examples

Bad:

```cpp
<!-- chrome/app/generated_resources.grd -->
<message name="IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_ALLOW_BUTTON_TOOLTIP" desc="Tooltip for allow button.">
  Allow extension to read and change this site
</message>

// ExtensionsMenuViewBinder.java
View allowButton = itemView.findViewById(R.id.allow_button);
// Missing tooltip setup for Android hover/large-screen accessibility
```

Good:

```cpp
<!-- chrome/app/generated_resources.grd -->
<message name="IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_ALLOW_BUTTON_TOOLTIP" desc="Tooltip for allow button." formatter_data="android_java">
  Allow extension to read and change this site
</message>

// ExtensionsMenuViewBinder.java
View allowButton = itemView.findViewById(R.id.allow_button);
TooltipCompat.setTooltipText(
    allowButton,
    context.getString(
        R.string.extensions_menu_requests_access_section_allow_button_tooltip));
```
