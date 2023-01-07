# Saved Desks

Saved Desks refers to any features which involve saving the active desk with all
its applications and then launching the saved desk via a UI in overview mode
with all its applications in their saved states. There are two such features;
Desk Templates and Save and Recall. They have a few differences but both involve
saving a desk and launching it at a later time. Save and Recall is available for
all users while Desk Templates has to be turned on via policy or a flag: "enable-desks-templates".

[TOC]

## User Journey

#### Overview Mode

The user starts off in overview mode - `ash/wm/overview/`. There are two buttons
named "Save desk as a template" and "Save desk for later". Both buttons will
save the active desk and its applications; "Save desk for later", which is Save
and Recall will additionally close the active desk and all its windows. The
button will be disabled or hidden if the active desk cannot be saved.

#### Desk Storage Model

Saving a desk triggers a call to the model which serializes and stores the desk.
The model logic is in `components/desks_storage/`. There are two ways to store
the desk:

1. Using `DeskSyncBridge` which stores it in the cloud via Chrome Sync.
2. Using `LocalDeskDataManager` which writes it to a file.

Both models support the same functionalities and are interchangeable from code
in `ash/` standpoint.

#### Library Page

This is the main UI the user interacts with. It is a page within overview mode
accessed by pressing the "Library" button on the desks bar. It contains grids
of items with each item representing one saved desk. The item gives information
such as time, name and a visual representation of what applications and tabs
are saved in the form of application icons and tab favicons. Additionally, users
can use buttons and a textfield on the item to launch, delete or update the
corresponding saved desks. There are also dialogs and toasts to assist users
with using the features.

#### Launching Applications

Launching applications is done via `ChromeDesksTemplatesDelegate`, which lives
in `chrome/`. Launching applications requires dependencies which are forbidden
in `ash/`, such as app service, profiles and browser code. The delegate is also
used to communicate to LaCros to launch LaCros browsers if LaCros is enabled.

Launching applications code is shared with the full restore feature. There are
a couple differences, including:

1. Support to move applications which only support a single instance and are
   already open.
2. Browser windows are created from scratch; full restore uses session restore
   to relaunch browser windows.

Launched templates have an associated `SavedDesk` object, which contains the
info necessary to launch the associated applications. The info is parsed by the
model into a `app_restore::RestoreData` object, which is part of the
`components/app_restore` library. This library is also used by full restore and
contains the logic to create the application widgets with the correct bounds.
Just like full restore, additional `ash/` logic like MRU order and window states
will be handled in `WindowRestoreController`.

## Differences

Though the two features are closely related and share a lot of code, they have a
couple differences. Desk templates is aimed towards reducing manual setup for
repetitive tasks, and templates created by an admin. Desks and templates are not
automatically deleted when using this feature. Save and recall is aimed towards
picking up where a user left off. Desks are deleted once saved, and templates
are deleted once launched. This reduces the number of user interactions when
dealing with the virtual desks limit and the saved desks limit.
