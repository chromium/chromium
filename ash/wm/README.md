# Window Manager

## Overview

This is the ash window manager, which allows users to manipulate and/or modify
windows. Windows are normally described as a `views::Widget` which has an
associated `aura::Window`. The windows managed are application windows and are
parented to a switchable container.

## Notable classes

#### MruWindowTracker

`MruWindowTracker` allows us to grab a list of application windows in most
recently used order. This will only grab windows in the switchable containers
and filters can be applied. There are some commonly used filters, these are
split into helper functions. The MRU list can be accessed anywhere in ash code.

```cpp
#include "ash/wm/mru_window_tracker.h"

auto windows = Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
```

#### WindowResizer

`WindowResizer` is the base class for window drag and resize logic. A subclass
of this, depending on the scenario may be created by
`ToplevelWindowEventHandler` when a valid event is seen. The event handler will
then forward the events to the subclass. The subclass will then handle
positioning and resizing the window, as well as creating supporting UIs (i.e.
phantoms) and possibly changing the window state on drag end.

#### WindowState

`WindowState` is a wrapper around the window's `aura::Window` that lets us
modify and query info about a window's state (i.e. maximized, minimized). It
also takes care of animations when changing states. It owns a `State` object
that can be overridden for different modes (ie. `ClientControlledWindowState`
for ARC apps, `TabletModeWindowState` for all other apps in tablet mode).
Helpers exist for common state changes, less common state changes can be sent
`WMEvent`'s. The `WindowState` of a window can be accessed anywhere in ash.

```cpp
#include "ash/wm/window_state.h"

WindowState* window_state = WindowState::Get(window);
WindowSnapWMEvent wm_event(WM_EVENT_SNAP_PRIMARY);
window_state->OnWMEvent(&wm_event);
// WindowState will compute the animation and target bounds and animate the
// window to the left half.
```
## Features

The following are features that are larger or more complex, or have many
interactions with non window manager features.

#### Desks

Desks is a productivity feature that allows users to place windows on a virtual
desk. Only the windows associated with the active desk will be visible. On
switching desks, the windows associated with the old active desk slide out, and
the windows associated with the new active desk slide in. Desks can be created,
accessed and destroyed using accelerators or a desk UI in overview mode.

#### Float

Float is another productivity feature that allows users to place one window per
desk above others. This is done by moving the window to a container stacked
above the desk containers.

In tablet mode, floated windows have a fixed size and are always magnetized to
the corners but can be dragged to other corners. The can also be tucked by
flinging the window horizontally offscreen. You can bring the window back by
pressing on the UI provided while tucked.

#### Gestures

Gestures provide a quick way of doing window management. This folder contains
gesture centric features like the back gesture and touch pad gestures, but other
features can have gestures built in (i.e. overview swipe to close).

#### Overview

Overview mode, previously known as window selector is a mode which displays all
your current windows. It provides an entry to desks and splitview. In clamshell,
you can access it doing a 3-finger swipe down on the trackpad, or pressing F5.
In tablet, you can access by swiping up on the shelf.

#### Splitview

Splitview is a productivity feature that allows using two windows side by side
with no real estate wasted. It can be activated by drag-drop in overview, ALT+[
or ALT+] accelerators, or swiping up from the shelf in tablet mode.

#### Tablet Mode

`TabletModeController` contains the logic to determine when a user wants to use
the Chromebook as a tablet. It then notifies many observers (i.e. shelf, app
list, chrome browser) to make their UI's more touch friendly or account for the
lack of a keyboard. Some features are also tablet mode only. They can register
as an observer, or check `TabletModeController::InTabletMode`.

#### WindowCycleController

Window cycler, or ALT+TAB allows you to switch between windows and view
thumbnails of running windows. Tapping TAB or SHIFT+TAB while holding ALT allows
cycling through the UI. If the accelerator is tapped quick enough, the UI will
not be shown.

## Performance

Window management features commonly involve moving, fading or updating one or
many windows. The windows are usually large textures and on top of that, we may
need supporting UI (i.e. indicators, phantoms) which may also be large and need
to be animated. This can lead to poor performance on low end devices. If the
feature has many large moving parts, consider adding metrics (`ThroughputTracker` ,
`PresentationTimeRecorder`), adding a tast test and monitoring the dashboards.
