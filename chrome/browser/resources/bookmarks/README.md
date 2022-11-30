# Bookmark Manager

The bookmark manager (BMM) is a WebUI surface with a large amount of
functionality for managing bookmarks across a relatively simple UI. This
document serves as an overview of the page, both in terms of the page features,
as well as the code design built up to support it.

## Major features

The following are some of the main features of the BMM which impact the design
of the code:

* **Real-time updates**: The display updates in real-time in response to any
  change to the bookmark model, whether from the page itself or from any other
  part of the browser UI.
* **Item selection**: It is possible to select items using either the mouse or
  keyboard, using ctrl/shift in much the same way as with a powerful file
  browser like Windows Explorer.
* **Contextual commands**: All viewing/editing commands adapt to the current
  selection. Most commands have corresponding keyboard shortcuts.
* **Drag and drop**: It is possible to drag bookmarks to move them between
  folders. This works within the BMM, as well as between native UI and the BMM,
  and between two different BMM instances from different Chrome profiles.
* **Policy support**: Several policies are respected:

  - [EditBookmarksEnabled](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=EditBookmarksEnabled):
    Prevents all editing operations
  - [ManagedBookmarks](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ManagedBookmarks):
    Defines a folder of immutable bookmarks.
  - [IncognitoModeAvailability](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=IncognitoModeAvailability):
    Disables/force-enables opening bookmarks in Incognito

## Data-flow model

[Full design document](https://docs.google.com/document/d/1deh7jm-x95d_nWfvWqZAwwgzR6q_1SXIuRoNnQlqCqA/edit?usp=sharing)

The BMM uses a one-way data flow model that is somewhat different to other
Polymer WebUI pages. This model is inspired by [Redux](http://redux.js.org/),
with a simple layer which binds it to Polymer UI components. Designing our data
flow in this way has a few primary benefits:

* We have a single source of truth for the state of the page
* We have a well-defined interface for making changes to the state of the page
* UI components are able to directly read whatever state they need, removing the
  need for a chain of tedious, highly-coupled Polymer bindings to manage state.

The following is a brief overview of how the data-flow model works. Note that
this model is only used for front-end Polymer UI, and is entirely separate to
the backend BookmarkModel in C++.

### Store

`store.js` provides a singleton class (`bookmarks.Store`) which gives access to
a read-only state tree for the entire page (`store.data`).

Any Polymer UI element which wants to be able to data-bind values from the
global state should add the mixin `bookmarks.StoreClient` (in
`store_client_mixin.ts`). This mixin allows elements to `watch` particular
values in the store, and be automatically updated when they change.

### Actions

Actions are the only way to modify the state tree. An `Action` is a plain
Javascript object which describes a modification that should be made to the
state tree. All `Action` objects are created by functions in `actions.js`.

To actually modify the state, call `Store.dispatch(action)` (or
`StoreClient.dispatch(action)`). This tells the store to process the action and
notify UI elements about what has changed.

Changes to the persistent bookmarks backend are made by calling into the
bookmarks extension API. These then call back to the BMM through
chrome.bookmarks event listeners, which dispatch actions to change the
Javascript bookmark model, updating the page.

**Note**: There's also limited support for Actions which are processed
asynchronously. See `dispatchAsync` for details, but this probably needs more
work before they are generally useful.

### Reducers

Reducers describe how an action affects the page state. These are **pure
functions** located in `reducers.js` which take a state and an action, and
return a new state.

Importantly, reducers must never modify any part of the input state: instead,
they return new objects for any part of the state tree that has changed. By
doing this, it is possible for `StoreClient` to quickly check for what has
changed using `==`, and only notify Polymer about things that have actually
changed. Plus, these functions are easier to test and debug!

## Context menus and keyboard shortcuts

[Full design document](https://docs.google.com/document/d/1AUWpwaiHgYlnBWeKW8hgScGZvaGa2BImOeV-d4BF8QE/edit?usp=sharing)

Context menus and keyboard shortcuts are controlled by the `CommandManager`,
located in `command_manager.js`. This includes all logic for commands which
open/edit bookmarks, including:

* Determining which commands should appear in the context menu (and whether they
  are enabled or disabled).
* Triggering commands on keyboard shortcuts
* Showing appropriate UI (such as confirmation dialogs, toasts) when the command
  is executed.

An enum of all possible commands is defined in `constants.js`.

<!-- TODO(calamity):
## Drag and drop
-->
