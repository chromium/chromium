---
name: general/disambiguate-action-menu-accessibility-labels
rubric: testing
severity: should_fix
---

**Rule.** When defining accessible labels or string resources for action,
overflow, or three-dot menu buttons on UI surfaces containing repeated items
(such as lists, cards, tables, or promos), provide a distinct, context-specific
accessible label (e.g., 'More actions for <item>') rather than a generic label
like 'More actions'. Retain generic labels only for visual tooltips where item
context is already visually obvious.

**Why.** When multiple action or overflow menus with identical generic labels
exist on a page, screen reader users navigating by interactive controls cannot
discern which item or entity a menu operates on. Providing a context-specific
accessible label disambiguates each button for assistive technology while
preserving concise visual tooltips.

## Examples

Bad:

```cpp
<!-- Generic accessible label ambiguous when multiple items exist -->
<message name="IDS_PROMO_MENU_A11Y_LABEL" desc="Aria label for promo menu.">
  More actions
</message>

<cr-icon-button iron-icon="cr:more-vert"
    aria-label="$i18n{promoMenuTooltip}"
    title="$i18n{promoMenuTooltip}">
</cr-icon-button>
```

Good:

```cpp
<!-- Contextualized accessible label disambiguates control for screen readers -->
<message name="IDS_PROMO_MENU_A11Y_LABEL" desc="Aria label for promo menu.">
  More actions for promo
</message>

<cr-icon-button iron-icon="cr:more-vert"
    aria-label="$i18n{promoMenuA11yLabel}"
    title="$i18n{promoMenuTooltip}">
</cr-icon-button>
```
