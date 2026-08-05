# Component Spec: List Items

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **List Items** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The List Items component (also referred to as URL List Item or Rich Hover Button) is used to display rows of structured metadata. It typically includes a leading visual (such as a favicon, image, or folder icon), a title, an optional description/subtitle, optional badges/chips, and an optional trailing action icon. It supports interactive hover, active, and focus states.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `List Items` | `RichHoverButton` | `<cr-url-list-item>` |
| **Source Files** | [Figma Link: `280:28521`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-28521) | [`chrome/browser/ui/views/controls/rich_hover_button.h`](//src/chrome/browser/ui/views/controls/rich_hover_button.h) | [`ui/webui/resources/cr_elements/cr_url_list_item/cr_url_list_item.ts`](//src/ui/webui/resources/cr_elements/cr_url_list_item/cr_url_list_item.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant | C++ `RichHoverButton` Method / Property | WebUI `<cr-url-list-item>` Property |
| :--- | :--- | :--- |
| **Size (Large, Medium, Compact)** | Governed by Layout / `CalculatePreferredSize` | `size` attribute (`"compact"`, `"medium"`, `"large"`) |
| **Type (Website, Folder)** | Differentiated via `SetIcon()` / Icon logic | `isFolder_` property |
| **hasImage (true/false)** | `SetIcon(ui::ImageModel)` | `imageUrls` (array) |
| **hasChips (true/false)** | `SetCustomView()` (inserts into custom row) | `<slot name="badges">` & `hasBadges` attribute |
| **Title / Subtitle** | `SetTitleText()`, `SetSubtitleText()` | `title` and `description` / `descriptionMeta` |
| **Trailing Action** | `SetActionIcon()` | `<slot name="suffix">` |

---

## 3. Component States

| Interactive State | Figma | C++ `RichHoverButton` (via `HoverButton`) | WebUI `<cr-url-list-item>` |
| :--- | :--- | :--- | :--- |
| **Default** | `state="Default"` | `ButtonState::STATE_NORMAL` | Standard layout without pseudo-classes |
| **Hovered** | `state="Hovered"` | `ButtonState::STATE_HOVERED` (InkDrop applied) | `:host(.hovered)`, `:host([force-hover])` |
| **Pressed / Active** | N/A | `ButtonState::STATE_PRESSED` | `:host(.active)` / `var(--cr-active-background-color)` |
| **Focused** | N/A | `views::FocusRing` applied to button | `:host-context(.focus-outline-visible):host(:focus-within)` |
| **Disabled** | N/A | `ButtonState::STATE_DISABLED` | `[disabled]` attribute |

---

## 4. Design Token Comparison (Side-by-Side)

| Token Type | Figma Property | C++ Views | WebUI Variable / CSS |
| :--- | :--- | :--- | :--- |
| **Item Height (Large)** | 68px | Calculated via views layout | `--cr-url-list-item-height` (68px) |
| **Item Height (Medium)** | 48px | Calculated via views layout | `--cr-url-list-item-height` (48px) |
| **Item Height (Compact)** | 36px | Calculated via views layout | `--cr-url-list-item-height` (36px) |
| **Padding (Large/Medium)** | 6px 16px (or 4px 16px) | Insets | `--cr-url-list-item-padding` |
| **Hover Background** | Overlay (#1f1f1f / opacity) | `kColorHoverButtonBackgroundHovered` | `--cr-hover-background-color` |
| **Active Background** | Overlay | Derived from Native Theme | `--cr-active-background-color` |
| **Asset Size (Large)** | 56px | Icon resolution | `height: 56px; width: 56px` |
| **Asset Size (Medium)** | 40px | Icon resolution | `height: 40px; width: 40px` |
| **Asset Size (Compact)** | 24px | Icon resolution | `height: 24px; width: 24px` |
| **Asset Corner Radius** | 4px (Compact), 8px (Med/Lrg) | Handled by ImageView | `border-radius: 4px` / `8px` |

---

## 5. Architectural & Implementation Gaps

* **Layout Structure**: C++ `RichHoverButton` utilizes an internal `views::TableLayout` to arrange its children (Icon, Title, State Image, Action Icon, Subtitle, and Custom Views). WebUI `<cr-url-list-item>` uses CSS Flexbox and Grid, taking advantage of standard DOM slots for prefix, suffixes, custom icons, and badges.
* **Anchor vs Button**: WebUI's `<cr-url-list-item>` explicitly handles an `asAnchor` property to swap between a native HTML `<a>` tag and `<button>` for semantic interactions, depending on whether it needs to invoke browser navigation natively or a custom action. `RichHoverButton` is strictly a `views::Button`.
* **State & Data Management**: WebUI supports multiple images natively through `imageUrls` and custom logic to arrange them in a grid for folders, whereas the C++ `RichHoverButton` expects pre-composed `ui::ImageModel`s for its leading icon.

---

## 6. Styling, Variants, Features and States Mismatches

* **Folder Visuals**: WebUI defines explicit folder-specific classes (`.folder-and-count`, `.folder-image`) to arrange up to four images into a quadrant for "folder" variants, complete with a badge overlay displaying count. C++ `RichHoverButton` does not have built-in folder-multi-image quadrant rendering; callers must construct a custom `ui::ImageModel` or `views::ImageView` that pre-composes this visual.
* **Component Sizing**: Figma strictly categorizes components via "Size" (Large, Medium, Compact). WebUI directly maps this via the `size` property. C++ `RichHoverButton` typically resizes based on the content injected (e.g., whether it has a subtitle) and inherited layout constraints, rather than an explicit `size` enum.
* **Subtitle Wrapping**: `RichHoverButton` has an explicit boolean `GetSubtitleMultiline()` to enable wrapping of description text, whereas WebUI governs this primarily through CSS flex and grid parameters.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- **Content Density**: Use the Compact size for high-density lists (like bookmarks) and Large for items with rich metadata (like history or tab journeys).
- **Favicons & Images**: Provide fallbacks when URLs or images fail to load (e.g., a default globe icon or folder icon).

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- **WebUI a11y rules**: `<cr-url-list-item>` properly manages `aria-label` and `aria-description`. Use the `asAnchor` property when the action triggers a navigation context to ensure standard right-click/middle-click behaviors are preserved.
- **C++ Views native focus**: Managed through `HoverButton::GetViewAccessibility()`. Ensure that `AddExtraAccessibleText()` is used to announce secondary information.

### 3. Icon Usage Guidelines
- The trailing action slot is typically reserved for `more_vert` (options menu) or `cancel` (remove item). Ensure adequate touch targets for secondary actions.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    `views::View` → `views::Button` → `views::LabelButton` → `HoverButton` → `RichHoverButton`
*   **WebUI (Web Frontend)**:
    `HTMLElement` → `LitElement` → `CrUrlListItemElementBase` (with `MouseHoverableMixinLit`) → `CrUrlListItemElement` (`<cr-url-list-item>`)
