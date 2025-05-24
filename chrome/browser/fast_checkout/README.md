# Fast Checkout

Fast Checkout is an Android-only feature to fill both the address and the
payments form in a checkout funnel with a single user interaction.

To achieve that, Fast Checkout shows a bottomsheet after the user interacts
with the first form in a recognized checkout flow. On that bottomsheet, the
user confirms both their address and their payments data. Submitting the
bottomsheet triggers filling both forms, potentially across navigations. Fast
Checkout does not navigate itself. Instead, it observes newly parsed forms
and fills the relevant ones after they become available.

## High-level architecture

- `FastCheckoutTabHelper` listens for navigations. If a URL is a checkout URL,
  asks `FastCheckoutCapabilitiesFetcher` to update its list of supported
  domains. `FastCheckoutTabHelper` is created for every `WebContents`.
- `FastCheckoutCapabilitiesFetcher` obtains supported checkout funnels from a
  remote server and keeps them in cache. It is a `KeyedService` with a lifetime
  mirroring that of the user's `Profile`.
- `FastCheckoutClient` contains most of the actual logic. It is notified about
  user interactions with form fields (and can intervene to show UI), learns
  about navigations and newly seen forms, and triggers filling.
  It is owned by `ChromeAutofillClient` and has, thus, approximately the
  lifetime of a `WebContents`.