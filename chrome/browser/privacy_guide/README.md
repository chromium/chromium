# Privacy Guide

This directory contains the Privacy Guide feature.

The Privacy Guide is a step-by-step guide, accessible from the Privacy and
Security section of Chrome's settings, to help users understand and manage
their privacy settings. The main goal of the Privacy Guide is to make key
privacy controls more accessible and understandable for everyday users.

## Key Components

The Privacy Guide is implemented as a series of cards, each corresponding to a
specific privacy setting. The main components are:

-   **Desktop**: The user interface for Desktop is built using WebUI and is
    located in
    `chrome/browser/resources/settings/privacy_page/privacy_guide/`.
-   **Android**: The Android-specific implementation is located in the `android/`
    subdirectory.
-   **Metrics**: The feature is instrumented with metrics to track user
    interactions and settings changes. The metric enums are defined in
    `privacy_guide.h`.
