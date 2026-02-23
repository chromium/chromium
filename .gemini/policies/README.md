# gemini-cli policies

This folder is gitignored, with specific checked in files exempted. Developers
are encouraged to put their personal chromium-specific policies in this same
folder. Other policies in this folder, with a higher priority, can override the
checked-in ones if you wish to bypass a checked in policy.

For checked in policies:
 * These policies have a high bar - they should be applicable to all developers
   in pretty much all scenarios.
 * Never use a priority of 999, since it cannot be overriden by a developer just
   for their chromium checkout.
 * `blocklist.toml` contains any commands which gemini should never run while
   developing Chromium.
