## Window Restore

Window restore contains the logic to set certain window management properties
such as (window state, mru order, bounds, etc.) once a window has been launched
via full restore, save and recall, or desk templates.

## Informed Restore Dialog

If the user has selected "Ask every time" in the full restore settings, on user
login, we will enter overview and display the informed restore dialog. It gives
the user a visual representation of the window that will be launched from full
restore, and a chance for users to cancel restoring. The visuals could either be
a screenshot, or apps and favicons to denote the last sessions' windows.
