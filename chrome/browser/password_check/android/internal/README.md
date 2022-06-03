# Password Check UI Component

This folder contains the internal parts of the Password Check component. Files,
classes and methods defined in here are not meant to be used outside of this
package.

This document provides a brief overview of the architecture.

[TOC]


## Component structure

This component follows the typical
[MVC](https://en.wikipedia.org/wiki/Model%E2%80%93view%E2%80%93controller)
structure that is widely used in Chrome on Android. The MVC structures separates
logic from representation:

 * The [controller](#Controller) creates and connects all subcomponents and
   performs logic that affects the visual appearance of the component.
 * The [model](#Model) keeps all state that affects the visual appearance of the
   component. Changes made to it are automatically propagated to the view by a
   Model-Change-Processor (MCP) as defined in
    `//src/ui/android/java/src/org/chromium/ui/modelutil/`
 * The [view](#View) is the representation of the component. It enforces styles
   and is mostly called by a view binder to set mutable properties.


A typical request to the Password Check component API that intends to change the
visible appearance involves all component parts. For example if the API caller
requests to show compromosed credentials:

1. The API caller asks the controller to show a set of
   `credentials`.
    1. The controller prepares and filters the `credentials` for display.
    2. The controller writes the prepared credentials into the [model](#Model)
       as `ITEMS`.
2. The MCP picks up model changes.
    1. The MCP identifies that the `ITEMS` property was changed.
    2. The MCP uses a ViewBinder to bind each settings item to the
       corresponding view (e.g. a `COMPROMISED_CREDENTIAL` item id would create
       or reuse a TextView with a button of options).
3. The view renders the changed credential list.
    1. The view may apply style, RTL settings and click event handlers.


## Model

The model holds state and event listeners connected to the view. An MCP
automatically notifies the listener about any change made to a property. To
automate this Observer structure, the model is a `PropertyModel` as defined in
`//src/ui/android/java/src/org/chromium/ui/modelutil/`. It is build by defining
properties and constructing a model with them. The properties are located in the
static `PasswordCheckProperties` class.

The model contains writable and readable properties. The readable properties are
guaranteed to never change for the lifetime of the Password Check component:

 * **ITEMS** which is the set of displayed credentials. The list
   itself will be modified (credentials will be added and removed) but the
   object remains the same which allows to permanently bind it to a list
   adapter.
 * **FRAGMET_EVENT_LISTENER** which is the listener that reacts to view events.
   The mediator implements this interface.

The writable properties change over the course of the components lifetime:

 * None so far


## Controller

The controller of this model implements the PasswordCheckComponent interface as
defined in `public/` and contains all logic that affects the component's visual
appearance. The controller consists of two parts:

  * **PasswordCheckCoordinator** which implements the public interface and
    creates all parts of the component (model, mediator, view, etc.) and links
    them using MCPs.
  * **PasswordCheckMediator** which handles request to the component API and
    changes the model accordingly.


## View

The view contains all parts that are necessary to display the check UI. It
consists of two parts:

 * **PasswordCheckViewBinder** which maps model changes to the view. For the
   Password Check component, the ViewBinder also supports an Adapter that binds
   changes to the `ITEMS` property to the RecyclerView inside the
   bottom sheet.
 * **PasswordCheckSettingsView** which displays all visible elements. Since it
   is a fragment that the SettignsLauncher needs to access, it is public.
