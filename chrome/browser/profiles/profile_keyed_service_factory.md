# ProfileKeyedServiceFactory Overview

This document present the newly added intermediate interface
`ProfileKeyedServiceFactory` that is used to create instances of`KeyedService`
more efficiently and with more control for each profile types.

[TOC]

## Purpose and implementation decisions

[go/profile-keyed-services](https://goto.google.com/profile-keyed-services)
(Google Internal)

## Description

`ProfileKeyedServiceFactory` is an intermediate interface to create
KeyedServiceFactory under chrome/ that provides a more explicit way of creating
services for non regular profiles.

The main purpose of this class is to provide an easy and efficient way to set
the redirection logic for different profile types using a `ProfileSelections`
instance that contains the logic of profile redirection. Those profile choices
are overridable by setting the proper combination of `ProfileSelection` and
Profile type in the `ProfileSelections` instance passed in the constructor.

This interface also helps in changing the default behavior of creating
services for non regular profiles. Previously, most services used to create
services with no checks on profile types, meaning that most existing services
are created for mostly all profile types. Changing the default behavior per
profile type allows for a clearer and better targeted usage of KeyedServices.

This work is mirrored on to `RefCountedProfileKeyedServiceFactory` for keyed
services that are ref counted.

## Profile types

There are two implementations of `Profile`:
- Original Profile (aka `ProfileImpl`): A profile which information are stored
and can be reused (usually a user profile).
- Off The Record (OTR) Profile (aka `OffTheRecordProfileImpl`): Owned and tied
to an Original Profile, this profile is never saved on disk.

There are different types of Profiles that can be both Original and OTR:
- Regular Profile: is a normal user profile.
- Incognito Profile: is the Primary OTR of a Regular Profile.
- Guest Profile: The OTR profile for a Guest is the profile used for browsing,
since no information is saved. The equivalent Original Profile is a dummy
profile that exists only to hold on to the OTR Profile, it is never used.
- System Profile: The OTR profile for a System profile is used to display the
Profile Picker, there are no information to save. The equivalent Original
Profile is a dummy Profole that exists only to hold on to the OTR Profile, it is
never used.
- Signin Profile: Part of the Ash internal profiles. (More information needed)
- LockScreen Profile: Part of the Ash internal profiles. (More information
needed)
- LockScreenApp Profile: Part of the Ash internal profiles. (More information
needed)
- Other Types? (More information needed)

### Platform specific considerations

- Ash has additional "irregular" profiles that are not used for actual browsing
(e.g. signin profile, lock screen profile).
- Android and Ash don't have system profiles.
- Android and Ash don't support multiple regular user profiles.
- Ash has a "device-level" guest mode where all the profiles are
guest. `IsGuestSession()` returns true for all profiles, and is no longer
exclusive with other profile type getter functions.
- iOS does not use profiles at all. It has another subclass of BrowserContext
(called BrowserState) and do not use any other profile types described above.

## ProfileSelections

Note that this structure is independent of the `ProfileKeyedServiceFactory`
interface and can be used in any other context that needs to select a profile
based on its type.

`ProfileSelections` is a helper structure that maps a ProfileType to a
redirection logic for that profile. A `ProfileSelection` attribute describes how
to pick the appropriate profile based on the input profile type.
`ProfileSelections` manages these choices and returns the proper profile.

This structure has one main public method that returns the selected profile
based on the `ProfileSelection` choice:
```c++
Profile* ProfileSelections::ApplyProfileSelection(Profile* profile);
```
Effectivley this method extracts the type of the input profile, searches for the
equivalent `ProfileSelection` attribute, apply it to the profile, and return the
profile that results of the selection.

`ProfileSelections` supported types:
* Regular Profile.
* Guest Profile.
* System Profile.
* Ash Internal Profiles.

`ProfileSelection` attributes description:
* `kNone`: Always returns nullptr, no profile is selected.
* `kOriginalOnly`: Only return the profile if it is an Original Profile, OTR
profiles are filtered out (returning nullptr).
* `kOwnInstance`: Always returns itself, both Original and OTR profiles.
* `kRedirectedToOriginal`: Always returns the Original profile, if the input is
the Original profiles, it returns itself, if the input is an OTR profile, it
returns it's equivalent Original profile.
* `kOffTheRecordOnly`: Only return the profile if it is an OTR profile, Original
profiles are filtered out (returning nullptr).

### Creating a ProfileSelections instance

`ProfileSelections` can only be created through a builder,
`ProfileSelections::Builder` which provides a method to properly set the
`ProfileSelection` choices for each supported profile type.

The default values that are set are:
- Regular Profile: kOriginalOnly.
- Guest Profile: kNone
- System Profile: kNone
- Ash Internal Profiles: Follows Regular Profile value.

`ProfileSelections` also has some predefined static builders in its interface
that provides common usages with standard behaviors.

### Future work
It seems that the Ash internal profiles are very similar to the System Profile,
meaning that similar cleanup for the services is potentially possible. More
analysis on this will be done and an experiment might be launched to transition
this type to be using `ProfileSelection::kNone` as well.
