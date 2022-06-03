General hierarchy of UI elements in authentication UI:

`LockScreen` is the root element, that  owns `LockContentsView` (potentially
wrapped in `LockDebugView`). It resides in kShellWindowId_LockScreenContainer
layer of the primary display.

`LoginDataDispatcher` implements `LoginScreenModel` and redirects calls to its
observers, main of which is `LockContentView`.

`LockContentView` is a full-screen view that owns and displays all other
authentication UI elements:
  * When only one user is in the list it is displayed using `LoginBigUserView`;
  * When two users are on the list, they are displayed using two
    `LoginBigUserView`s;
  * When 3+ users are in the list, one `LoginBigUserView` is used to display
    selected user, and rest of the users are displayed using
    `ScrollableUsersListView`;
  * `LoginExpandedPublicAccountView` when the user tries to sign in to public
    account.
      * Allows selection of language/keyboard for Public session
      * Displays monitoring warning indicator and triggers
        `PublicAccountWarningDialog`
      * Allows to actually sign in to the public account
  * Also owns/refers to following optional UI elements:
      * `LockScreenMediaControlsView`
      * `NoteActionLaunchButton`
      * UI that shows information about system.
      * Various bubbles and indicators
          * `UserAddingScreenIndicator` displayed when screen is used for
            multiprofile login
          * `BottomStatusIndicator` for indicating management/ADB sideloading
            info
          * `ManagementBubble` for management disclosure
          * `AuthErrorBubble` for displaying auth errors
          * `LoginErrorBubble` for displaying:
              * security warnings when detachable keyboard does not match one
               used on previous login
              * ext4 migration warning
              * supervised user deprecation notice
          * `LoginTooltipView` for easy unlock tooltips

`LoginBigUserView` contains one of:
  *   `LoginPublicAccountUserView` that consists of:
      * `LoginUserView` (see below)
      * Arrow button to proceed to showing `LoginExpandedPublicAccountView`
        (see above)
  *   `LoginAuthUserView` that represents user information and provides UI
       for authentication. It consists of:
      * `LoginUserView`
           * (Animated) user image
           * Name label
           * Drop-down info with an option to remove user
      * `LoginPasswordView` that shows:
           * Password input field
           * "Show Password" button
           * CAPS LOCK indicator
           * Submit button
           * Quick unlock indicator
      * or pair of `LoginPinView` (that provides digital keyboard) along with
        `LoginPinInputView` (that provides positional input field)
      * Password/PIN toggle button
      * Button to trigger online sign-in
      * `FingerprintView`
      * `ChallengeResponseView`
      * `DisabledAuthMessageView` (e.g. when child user has an associated
        time limit)
      * `LockedTpmMessageView`

`PinRequestWidget` works as a standalone UI. It contains a `PinRequestView`
that consists of `LoginPinKeyboard` and one of either `FixedLengthCodeInput` or
`FlexCodeInput`, both of which are subclasses of `AccessCodeInput`.
