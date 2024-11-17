# Touch To Fill Android Feature

This folder contains the internal parts of the Touch To Fill component. Files,
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


A typical request to the Touch to Fill component API that intends to change the
visible appearance involves all component parts. For example if the API caller
requests to show new credentials:

1. The API caller asks the controller to show a set of
   `credentials`.
    1. The controller prepares and filters the `credentials` for display.
    2. The controller writes the prepared credentials into the [model](#Model).
2. The MCP picks up model changes.
    1. The MCP identifies that the `CREDENTIAL_LIST` property was changed.
    2. The MCP uses a ViewBinder to bind each changed credential to the
       corresponding view (e.g. a TextView inside a RecyclerView).
3. The view renders the changed credential list.
    1. The view may apply style, RTL settings and event handlers for click events.


## Model

The model holds state and event listeners connected to the view. An MCP
automatically notifies listener about any change made to a property. To automate
this Observer structure, the model is a `PropertyModel` as defined in
`//src/ui/android/java/src/org/chromium/ui/modelutil/`. It is build by defining
readable and writable properties and constructing a model with them. The
properties (and a simple factory method for the model) are located in the static
`TouchToFillProperties` class.

The model contains writable and readable properties.The readable properties are
guaranteed to never change for the lifetime of the Touch To Fill component:

 * **VIEW_EVENT_LISTENER** which is the listener that reacts to view events. The
   mediator implements this interface.
 * **CREDENTIAL_LIST** which is the set of displayed credentials. The list
   itself will be modified (credentials will be added and removed) but the
   object remains the same which allows to permanently bind it to a list
   adapter.

The writable properties change over the course of the components lifetime:

 * **VISIBLE** which will trigger the component to render the bottom sheet or
   hide it, if it was visible.
 * **FORMATTED_URL** which is displayed as subtitle for the bottom sheet.


## Controller

The controller of this model implements the TouchToFillComponent interface as
defined in `public/` and contains all logic that affects the component's visual
appearance. The controller consists of two parts:

  * **TouchToFillCoordinator** which implements the public interface and creates all
    parts of the component (model, mediator, view, etc.) and links them using
    MCPs.
  * **TouchToFillMediator** which handles request to the component API and changes
    the model accordingly. Interactions with the view are typically handled here
    and either affect the model or notify callers of the component API.

## View

The view contains all parts that are necessary to display the bottom sheet. It
consists of two parts:

 * **TouchToFillViewBinder** which maps model changes to the view. For the
   Touch To Fill component, the ViewBinder also supports an Adapter that binds
   changes to the `CREDENTIAL_LIST` property to the RecyclerView inside the
   bottom sheet.
 * **TouchToFillView** which uses the BottomSheetController to create and
   modify the bottom sheet where credentials are displayed.
