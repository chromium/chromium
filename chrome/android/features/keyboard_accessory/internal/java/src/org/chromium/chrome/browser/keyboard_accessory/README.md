# Manual filling component

This folder contains all ui components that are necessary to display the
keyboard accessory bar and the accessory bottom sheet. They are mainly used
for autofill- and password-related tasks.

## Use cases

1. Act as an autofill popup substitute by displaying all autofill suggestions as
   as chips above the keyboard.
2. Provide an entry point to password generation (automatic and manual).
3. Provide fallback sheets to fill single form fields with stored password,
   address or payments data.

## Structure

The ManualFillingCoordinator in this package uses the `bar_component.*` to
display a bar above an open keyboard. This bar shows suggestions and holds a
number of fallback icons in a `button_group_component.*` which allows to open an
accessory sheet with fallback data and options.
The sheet is located in the `sheet_component.*` and shows one of the fallback
sheets as defined in `sheet_tabs.*`.
The responsibility of the ManualFillingCoordinator is to integrate the active
sub components with the rest of chromium (e.g. Infobars, popups, etc.) and
ensure that they are perceived as extension or replacement of the keyboard.

The `data.*` package provides helper classes that define the data format used by
all components. They support data exchange by providing generic `Provider`s and
simple implementations thereof.

### Manual Filling Component as State Machine

The filling component maintains exactly one state that determines how the sub
components behave. It is stored in `keyboard_extension_state` and is modified by
a number of signals. One example:

1. The component is in the HIDDEN state.
1. The signal `showWhenKeyboardIsVisible()` sets the state to `FLOATING_BAR`.
1. The component checks in `meetsStatePreconditions()` whether the set state
   fulfills all state-dependent preconditions (if so, it would transition into
   the `HIDDEN` state instead.)
1. In `enforceStateProperties`, the filling component modifies the subcomponents
   according to the new state which means it:
   1. shows the keyboard accessory bar
   1. hides any fallback sheets (noop since there is none)
1. Now the component reserves the bottom space (to make sure the bar doesn't
   cover content)
1. Finally, the component requests to show the keyboard (noop since it happens
   anyway).

At any point during that flow (or shortly after), the keyboard would trigger
which sets the `keyboard_extension_state` to `EXTENDING_KEYBOARD`. Since the
states have an exact mapping for all sub components, the exact timing isn't
relevant and even if the keyboard doesn't appear (e.g. in multi-window mode or
due to hardware keyboards), the filling component remains in a consistent state.

Any state can transition to a number of different states. States that can be
entered from any state are only:

* `EXTENDING_KEYBOARD` which attaches a bar to an opened keyboard.
* `HIDDEN` which hides sheet and bar (for a variety of reasons).

States that are entered following user interactions are visible in the table
below that also shows what effects each state has on a particular sub component.
The "Floats" column basically means that this state will ask for a keyboard
since these untethered states either:

* leave sufficient room for a keyboard,
* are merely a transition state into `EXTENDING_KEYBOARD`, or
* couldn't show a keyboard anyway (because multi-window/hardware suppresses it
  but Chrome doesn't know that beforehand)

|   ID   | State                 | Accessory Bar            | Fallback Sheet                          | Floats  | Transition into*
|--------|-----------------------|--------------------------|-----------------------------------------|---------|-
| 0x0100 | HIDDEN                | Hidden                   | Hidden                                  | N/A     | FLOATING_BAR, REPLACING_KEYBOARD
| 0x0101 | EXTENDING_KEYBOARD    | **Visible**              | Hidden                                  | No      | WAITING_TO_REPLACE
| 0x0000 | WAITING_TO_REPLACE    | Hidden                   | N/A — waits for keyboard to (dis)appear | No      | REPLACING_KEYBOARD
| 0x0010 | REPLACING_KEYBOARD    | Hidden                   | **Visible**                             | No      | FLOATING_SHEET
| 0x1101 | FLOATING_BAR          | **Visible**              | Hidden                                  | **Yes** | FLOATING_SHEET
| 0x1010 | FLOATING_SHEET        | Hidden                   | **Visible**                             | **Yes** | FLOATING_BAR

\* Excluding HIDDEN and EXTENDING_KEYBOARD which can be entered from any state.

### Using providers to push data

The manual filling component cannot verify the correctness of displayed
suggestions or determine exactly when they arrive. It is only responsible for
showing/hiding subcomponents ensuring that the space they consume plays well
with keyboard, content area and other Chrome UI.
The number of providers varies by sub component:

* Each fallback sheet has one provider (1:1 mapping to a
  `ManualFillingComponentBridge`).
* The keyboard accessory can handle multiple provides (arbitrary number but
  at most one per `AccessoryAction`, each of which currently maps to either a
  `ManualFillingComponentBridge` or a `AutofillKeyboardAccessoryViewBridge`).

This opens up a problem since the manual filling component is shared in the
`ChromeActivity` but the bridges exist once per tab
(`ManualFillingComponentBridge`) or even once per frame
(`AutofillKeyboardAccessoryViewBridge`) and send their data only once, even if
the tab isn't active.

Therefore, the manual filling component keeps a `ManualFillingState` for each
known `WebContents` object inside the `ManualFillingStateCache`. Based on that
state, the filling component only allows to forward data from providers that
push data for the active tab (i.e. per WebContents).
Data that is pushed to inactive tabs might need to be rerequested if the tab
changes (see [Caching](#caching) below.).

## Development

Ideally, components only communicate by interacting with the coordinator of one
another. Their inner structure (model, view, view binder and properties) should
remain package-private. For some classes, this is still an ongoing rework.

### Known places of confusion

The component has a couple of historical issues that are not resolved (yet) and
keeping them in mind until they are fixed simplifies working with it:

* Scope of the manual filling component:
    * The **ManualFillingComponent is browser-scoped** and exists only once
      after it is instantiated by the `ChromeActivity`.
    * The **fallback sheets are WebContents-scoped** which starts with the
      `ManualFillingComponentBridge` and is true for native controllers as well.
      Each `WebContents` objects maps to one tab. Since a tab may have multiple
      frames with different origins, some sheets (like passwords) have
      frame-specific content despite being WebContents-scoped.
    * The **keyboard accessory suggestion are frame-scoped**. Since the manual
      filling component has no understanding of frames, it is expected to always
      treat accessory suggestions with absolute priority.

* The fallback sheets are often referred to as "tabs". This is because each
  sheet is a tab of a `ViewPager` and the very early keyboard accessory had no
  notion of browser tabs. Ideally, we would use "sheet {types,icons}" instead.

* The filling component has two "states":
    * the `keyboard_extension_state` describes the visibility of sub components
      (e.g.setting it to `EXTENDING_KEYBOARD` shows the accessory but no sheets)
    * the `ManualFillingState` is a cache object that keeps the contents and
      wiring for accessory sheets per tab.
  It's unclear how to resolve this yet but preferably, the `ManualFillingState`
  could receive a less generic name once it it's not used to store sheet content
  anymore.

* Despite the name, the manual filling component is not fully modularized since
  it still requires a dependency to `chrome_java`. Ideally, the entire component
  would follow the folder structure of a typical component as well. All of this
  is a WiP, see https://crbug.com/945314.
