# Accessibility Implementation Patterns

Use these patterns to guide the refactoring of custom accessibility
announcements and text-based state updates.

The following are a list of potential patterns (non-exhaustive) that may help
drive discovery of additional issues in Clank.

______________________________________________________________________

## Pattern: Expandable preference/widget

See the git commit in history with the hash
`77621e23dd5ac247cc238ee44209df0f116f92ac` for an example of this pattern.

### Bad Pattern (Text-Based State)

In this pattern, the developer concatenates text like "Expanded" or "Collapsed"
to the preference title and sets it as the `contentDescription`. A custom
`AccessibilityEvent` is then fired to force TalkBack to read the updated text.

```java
// ExpandablePreferenceGroup.java
View view = holder.itemView;
String description =
        getTitle()
                + getContext()
                        .getString(
                                mExpanded
                                        ? R.string.accessibility_expanded_group
                                        : R.string.accessibility_collapsed_group);
view.setContentDescription(description);
if (view.isAccessibilityFocused()) {
    view.sendAccessibilityEvent(AccessibilityEvent.CONTENT_CHANGE_TYPE_CONTENT_DESCRIPTION);
}
```

### Good Pattern (Semantic Actions & Delegates)

Instead of mutating the `contentDescription`, we use an
`AccessibilityDelegateCompat` to set the expanded state and expose
`ACTION_EXPAND` or `ACTION_COLLAPSE` actions to the accessibility node info.

```java
// ExpandablePreferenceAccessibilityDelegate.java
package org.chromium.components.browser_ui.settings;

import android.os.Bundle;
import android.view.View;
import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;
import androidx.preference.Preference;
import java.util.function.BooleanSupplier;

public class ExpandablePreferenceAccessibilityDelegate extends AccessibilityDelegateCompat {
    private final Preference mPreference;
    private final BooleanSupplier mExpandedSupplier;

    public ExpandablePreferenceAccessibilityDelegate(
            Preference preference, BooleanSupplier expandedSupplier) {
        mPreference = preference;
        mExpandedSupplier = expandedSupplier;
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfoCompat info) {
        super.onInitializeAccessibilityNodeInfo(host, info);
        boolean expanded = mExpandedSupplier.getAsBoolean();
        info.setExpandedState(
                expanded
                        ? AccessibilityNodeInfoCompat.EXPANDED_STATE_FULL
                        : AccessibilityNodeInfoCompat.EXPANDED_STATE_COLLAPSED);
        info.addAction(
                expanded
                        ? AccessibilityActionCompat.ACTION_COLLAPSE
                        : AccessibilityActionCompat.ACTION_EXPAND);
    }

    @Override
    public boolean performAccessibilityAction(View host, int action, Bundle arguments) {
        if (action == AccessibilityActionCompat.ACTION_EXPAND.getId()
                || action == AccessibilityActionCompat.ACTION_COLLAPSE.getId()) {
            mPreference.performClick();
            return true;
        }
        return super.performAccessibilityAction(host, action, arguments);
    }

    public static void apply(
            Preference preference,
            View container,
            View title,
            BooleanSupplier expandedSupplier) {
        ExpandablePreferenceAccessibilityDelegate delegate =
                new ExpandablePreferenceAccessibilityDelegate(preference, expandedSupplier);
        ViewCompat.setAccessibilityDelegate(container, delegate);
        if (title != null) {
            ViewCompat.setAccessibilityDelegate(title, delegate);
        }
    }
}
```

And apply it in the preference class:

```java
// ExpandablePreferenceGroup.java
View view = holder.itemView;
View title = (View) holder.findViewById(android.R.id.title);
ExpandablePreferenceAccessibilityDelegate.apply(this, view, title, this::isExpanded);
```

______________________________________________________________________

## Pattern: liveRegion usage

### Bad pattern (Using assertive live regions or forcing announcements)

In this pattern, the developer uses `assertive` live regions or calls
`announceForAccessibility` to force TalkBack to speak every time a minor or
frequent UI change occurs (e.g., search autocomplete counts or progress
percentages). This interrupts the user's current reading flow and creates a
noisy experience.

```java
// Bad: Interruption of TalkBack speech for frequent or non-critical updates
progressBar.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_ASSERTIVE);

// Bad: Forcing a raw audio announcement when standard UI state changes are sufficient
view.announceForAccessibility("Loading completed");
```

### Good pattern (Using polite live regions and native view states)

For updates that are not critical enough to immediately interrupt the user, use
`polite` live regions. TalkBack will wait until the user stops navigating or
speaking before announcing the update.

```java
// Good: TalkBack announces changes politely when the user is idle
statusTextView.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
```

### Bad pattern (Live Region State Leaks - Missing NONE Reset)

In this pattern, a developer sets a view's live region to `polite` (or
`assertive`) to announce a one-off temporary state change (such as an animation
or a refresh). However, they never reset the live region when the action
finishes. This leaves the view permanently flagged as a live region, which can
cause unexpected and out-of-context announcements if the view's properties
change later for an unrelated reason.

```java
// Bad: Setting the live region for a temporary action but failing to reset it
iconView.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
iconView.animate().rotation(360f).start();
```

### Good pattern (Resetting temporary live regions)

If a view is not permanently intended to be a live region, it must be reset to
`NONE` as soon as the temporary event finishes (e.g., inside an animation's
`withEndAction` or `onAnimationEnd` callback). This strictly bounds the screen
reader's announcements to only the specific event that was intended.

```java
// Good: Resetting the live region after the temporary action completes
iconView.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
iconView.animate().rotation(360f).withEndAction(() -> {
    iconView.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_NONE);
}).start();
```

______________________________________________________________________

## Pattern: heading or paragraph semantics

### Bad pattern (Stylized headers without heading semantics)

In this pattern, section titles on long scrollable pages are styled using text
size, color, or bold formatting to look like headings, but they have no
accessibility properties. Screen readers treat them as plain body text, meaning
users cannot navigate the page by headings.

```java
// Bad: Visual heading only; invisible to heading-based screen-reader navigation
TextView sectionHeader = new TextView(context);
sectionHeader.setText("Privacy Settings");
sectionHeader.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20);
sectionHeader.setTypeface(Typeface.DEFAULT_BOLD);
```

### Good pattern (Explicit heading markers)

By setting heading semantics, TalkBack allows users to jump from section to
section by swiping up or down, making navigation of long pages much faster.

```java
// Good: Tells the accessibility framework that this view acts as a heading
TextView sectionHeader = new TextView(context);
sectionHeader.setText("Privacy Settings");
sectionHeader.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20);
sectionHeader.setTypeface(Typeface.DEFAULT_BOLD);
ViewCompat.setAccessibilityHeading(sectionHeader, true);
```

______________________________________________________________________

## Pattern: paneTitle and custom window transitions

### Bad pattern (Using liveRegions or requestFocus on overlay open)

When a popup window, dialog, or drawer appears, developers sometimes manually
call `announceForAccessibility` or use `liveRegion` to announce that the view
has appeared, or they call `requestFocus()` directly on the popup container.

This is a bad pattern because:

- Moving focus to a new container without informing the system of a window
  change breaks navigation flow.
- Direct `announceForAccessibility` calls can conflict with TalkBack's current
  state and get cut off.
- Proactively calling `requestFocus()` without system transition context can
  disorient screen-reader users.

```java
// Bad: Forcing a raw audio announcement when a popup opens
popupView.announceForAccessibility("Security Details dialog opened");

// Bad: Proactively forcing accessibility focus to the layout
popupView.setVisibility(View.VISIBLE);
popupView.requestFocus(); // Disorients users when done abruptly without a pane transition
```

### Good pattern (Using paneTitle and PANE_APPEARED window events)

The correct approach is to assign an accessibility pane title to the container
view and notify the accessibility framework using a `TYPE_WINDOW_STATE_CHANGED`
event with the subtype `CONTENT_CHANGE_TYPE_PANE_APPEARED`. The system will
announce the pane transition naturally, and TalkBack will automatically handle
focusing the first logical element inside the container.

```java
// Good: Define pane title and let the accessibility system manage the transition
ViewCompat.setAccessibilityPaneTitle(popupView, "Security Details");
popupView.setVisibility(View.VISIBLE);

// Notify the system that a new logical window/pane has appeared
AccessibilityEvent event = AccessibilityEvent.obtain();
event.setEventType(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_APPEARED);
popupView.sendAccessibilityEventUnchecked(event);
```

> [!NOTE] While `requestFocus()` should not be proactively called when opening a
> popup, it is still a best practice to **restore** focus to the triggering
> element once the popup is dismissed so that the user is returned to their
> previous context:
>
> ```java
> // Track the view that had focus before the popup was shown
> mTriggerView = hostActivity.getCurrentFocus();
> ...
> // When the popup is closed, restore focus
> if (mTriggerView != null) {
>     mTriggerView.requestFocus();
> }
> ```

______________________________________________________________________

## Pattern: Proactive Focus Manipulation

### Bad pattern (Moving the user's cursor arbitrarily)

Unless it is to restore focus to a previously focused element (e.g. after
dismissing a menu) or transitioning to an explicitly opened screen,
`requestFocus()` should never be proactively called, and an `AccessibilityEvent`
of type `AccessibilityEvent.TYPE_VIEW_FOCUSED` should never be sent to move the
user's focus cursor.

```java
// Bad: Grabbing focus automatically on page load or state change without user interaction
searchButton.requestFocus(); // Unexpectedly jumps the user's focus cursor
```

### Good pattern (Preserving natural focus flow)

Allow the accessibility framework to handle focus naturally as the user swipes
through elements on the screen.

```java
// Good: Let the user navigate to and focus the button naturally
// Do not call requestFocus() on views arbitrarily.
```

______________________________________________________________________

## Pattern: Overriding standard accessibility action labels

### Bad pattern (Custom action text on standard actions)

In this pattern, a developer overrides the default accessibility action labels
(like `ACTION_CLICK` or `ACTION_LONG_CLICK`) with custom action verbs (e.g.,
setting the click label to `"pay"` or `"open folder"`).

This is a bad pattern because:

- It alters the standard system template announcements ("double tap to
  activate"), making the interface inconsistent with the rest of the Android OS.
- It can lead to accessibility bugs or confusion for users who rely on standard,
  predictable announcements.
- **Corollary:** Developers might register custom, brand-new accessibility
  actions but forget to handle the action inside `performAccessibilityAction`,
  making the control unresponsive when invoked via accessibility tools.

```java
// Bad: Overriding standard action labels with custom text
AccessibilityActionCompat customClick = new AccessibilityActionCompat(
        AccessibilityActionCompat.ACTION_CLICK.getId(), "pay");
info.addAction(customClick); // TalkBack reads: "Double-tap to pay" instead of standard "activate"

// Bad: Creating custom actions without implementing the handler in performAccessibilityAction
AccessibilityActionCompat myAction = new AccessibilityActionCompat(R.id.my_action_id, "Reorder item");
info.addAction(myAction);
// (Missing corresponding implementation in performAccessibilityAction for R.id.my_action_id)
```

### Good pattern (Preserving standard actions and handling custom actions)

Allow the system to announce standard actions using default templates. If custom
actions are necessary (like custom swipe or reorder gestures that don't have
standard framework equivalents), always ensure they are fully implemented in
`performAccessibilityAction`.

```java
// Good: Let the system handle standard click/long-click announcements naturally
// No need to override ACTION_CLICK/ACTION_LONG_CLICK labels.

// Good: Register and handle custom actions correctly
AccessibilityActionCompat reorderAction = new AccessibilityActionCompat(R.id.reorder_action, "Reorder item");
info.addAction(reorderAction);

@Override
public boolean performAccessibilityAction(View host, int action, Bundle arguments) {
    if (action == R.id.reorder_action) {
        performReorder();
        return true;
    }
    return super.performAccessibilityAction(host, action, arguments);
}
```

## Rules and Guidelines

1. **Do NOT Modify API Contracts:** Some Android frameworks and custom Chromium
   components define overrides that require accessibility string resource IDs
   (e.g. `BottomSheetContent#getSheetClosedAccessibilityStringId()`). These are
   correct API contracts and should not be removed.

2. **Always Handle the Action:** If a specific action is enabled or invoked
   (e.g., `AccessibilityActionCompat.ACTION_EXPAND`), ensure
   `performAccessibilityAction` actually implements the logic to invoke the
   corresponding change (e.g., calling `performClick()` or running a state
   transition).

3. **Clean Up Unused Resources:** Once the hardcoded string-based announcements
   are removed, remove the unused strings (like
   `IDS_ACCESSIBILITY_EXPANDED_GROUP`) from the corresponding `.grd` or `.grsp`
   resource files.

4. **Manual Accessibility Events:** Creating and dispatching manual
   `AccessibilityEvent` objects is not inherently incorrect. It is often
   necessary for custom views or to announce pane transitions (like
   `CONTENT_CHANGE_TYPE_PANE_APPEARED`). Simply seeing a manual event is not a
   bug. However, audit manual events to ensure:

   - They are not used to bypass semantic framework APIs (e.g., sending text
     description updates instead of setting `setExpandedState`).
   - They are not used to proactively force focus shifts (such as sending
     `TYPE_VIEW_FOCUSED` arbitrarily).
