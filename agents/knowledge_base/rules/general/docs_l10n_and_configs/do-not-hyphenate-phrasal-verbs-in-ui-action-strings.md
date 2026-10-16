---
name: general/do-not-hyphenate-phrasal-verbs-in-ui-action-strings
rubric: readability
severity: should_fix
---

**Rule.** In user-facing UI strings, titles, and button labels (in .grd and
.grdp files), do not hyphenate phrasal verbs (such as 'Sign in', 'Sign up', 'Log
in', or 'Set up') when used as actions or calls to action. Reserve hyphenated or
compound forms (such as 'sign-in', 'sign-up', or 'setup') strictly for noun or
adjective usages.

**Why.** In English grammar, 'sign in', 'sign up', and 'set up' are two-word
phrasal verbs when expressing an action. Hyphenated or solid compound forms are
nouns or adjectives; using them as action verbs introduces grammatical errors
into user-facing copy and translation source text.

## Examples

Bad:

```cpp
<message name="IDS_FEDERATION_SIGN_IN_TITLE" desc="Title of the UI prompting the user to sign in." translateable="false">
  Sign-in with <ph name="IDP_NAME">$1<ex>google.com</ex></ph>
</message>
<message name="IDS_ACCOUNT_SIGN_UP_BUTTON" desc="Label for button to create a new account.">
  Sign-up now
</message>
```

Good:

```cpp
<message name="IDS_FEDERATION_SIGN_IN_TITLE" desc="Title of the UI prompting the user to sign in." translateable="false">
  Sign in with <ph name="IDP_NAME">$1<ex>google.com</ex></ph>
</message>
<message name="IDS_ACCOUNT_SIGN_UP_BUTTON" desc="Label for button to create a new account.">
  Sign up now
</message>
```
