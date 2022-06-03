# Shelf

This is the ash shelf, the system interface surface that allows users to launch
application shortcuts or go to the home screen, among other things.

## Components

The shelf contains the following components, each of which lives in its own
widget:

* The **shelf widget** contains no actionable UI but contains the semi-opaque
background shown behind the whole shelf as well as the drag handle (in certain
circumstances) to give users a hint that gestures can be performed. In that
sense, even though the shelf widget does not actually contain other components,
it usually serves as a backdrop for them.

* The **navigation widget** contains the home and back buttons. It is usually
shown in clamshell mode (but only with the home button) and hidden in tablet
mode, unless the activation of select accessibility features forces it to be
shown. When the navigation widget is not shown, the user can achieve the same
actions by performing gestures.

* The **hotseat widget** contains icons for application shortcuts and running
applications. In clamshell mode, it is always visually contained within the
shelf widget; in tablet mode, it can appear and move independently.

* The **status area widget** (whose code lives in `ash/system`) shows
information such as the clock or current battery level, and can toggle the
system tray.

## Alignment

The shelf is aligned to the bottom of the screen by default, but the user can 
choose (only in clamshell mode) to align it to the left or right of the screen. 
It always occupies the entirety of the corresponding dimension (width for a 
horizontal shelf, height otherwise), with the navigation widget shown at the 
start (top or left in left-to-right interfaces, bottom or right in 
right-to-left) and the status area at the other end.

## Auto-hiding

The system allows the user to set a boolean preference, on a per-display basis, 
specifying whether the shelf should "auto-hide". In that case, the shelf and its 
components will be hidden from the screen most of the time, unless there are no 
un-minimized windows or unless the user actively brings up the shelf with the 
mouse or with a swipe.

## Centering

The hotseat widget is centered on the screen according to the following
principle:

* All icons are placed at the center of the whole display if they can fit
without overlapping with any other shelf component.

* Otherwise, they are centered within the space available to the hotseat.

* If there are too many icons to fit in that space, the hotseat becomes
scrollable.

## Responsive layout

The shelf and its components need to adjust to a certain number of changes that
may or may not be user-triggered:

* Switching between clamshell and tablet mode.

* Changing the display size (for smaller displays, the shelf becomes more
compact) or orientation.

* Changing the shelf alignment.

* User events (clicks, taps, swipes).

### Coordination

All shelf components need to react to these changes in a coordinated manner to
maintain the smoothness of animations.

Components should not register themselves as observers of these changes and
react to them on their own, because an adequate reaction may involve other
components as well. For instance, whether the navigation widget is shown (or is
scheduled to be shown at the end of the animation) will influence the amount of
space the hotseat widget can occupy.

Instead, listening to those changes are handled at the `ShelfLayoutManager`
level, which is then responsible for making the changes trickling down to each
component as necessary.

### Aim first, move second

In reaction to any of these global changes, each component must first determine
where it wants to be at the end of the animation ("aim"). That calculation may
depend on the other shelf components. Then, and only then, should the change of
bounds be actually committed to each widget and the animations triggered
("move"). Failing to respect this "two-phase" approach may lead to janky
animations as each component may realize, only after it has started moving, that
another component's movement forces it to alter its final destination.

### `ShelfComponent` interface

Each of the shelf components exposes an API to other classes in order to ease
the process of responding to layout changes:

* `CalculateTargetBounds` is the "aim" phase, where each component figures out
where it wants to be given the new conditions. This method must be called on
each component by order of dependency (a component B "depends" on another
component A if B needs to know A's target bounds before calculating its own).

* `GetTargetBounds` allows for components depending on this one to calculate
their own target bounds accordingly.

* `UpdateLayout` is the "move" phase, where each component actually changes it
bounds according to its target.

* `UpdateTargetBoundsForGesture` allows each component to respond to a gesture
in progress by determining how (and whether) it should follow other components
along in the gesture.

### Layout inputs

Each shelf component is aware of the set of inputs that can cause its layout to
change. Each time the `UpdateLayout` method is called on it, it determines
whether any of its inputs has changed. If not, the method returns early and
avoids any actual re-layout for itself as well as other components that depend
solely on it.

## Keyboard navigation

In order for keyboard users to navigate smoothly between the various parts of
the shelf as they would expect, the `ShelfFocusCycler` class passes the focus to
each shelf component as appropriate, depending on which component has just
reliquished focus and on which direction the focus is going. The `ShelfWidget`
class is the only shelf component that doesn't receive keyboard focus since it
does not have any activatable elements.

## Buttons

The base class for all buttons on shelf components is `ShelfButton`, which
handles basic logic for keyboard navigation and ink drops. This class is then
derived into `ShelfControlButton` for things like the home or back button, and
`ShelfAppButton` for application shortcuts.


## Tooltips

Tooltips for elements on the shelf require some specific logic on top of the
common tooltips because as a user hovers over each app shortcut, trying to
figure out what each one does, we do not want to adopt the default tooltip
behavior which would be to dismiss the previous tooltip and make the user wait
for the common timeout before showing the next one.
