chrome/browser/ash
==================

This directory should contain Ash specific code that has `chrome/browser`
dependencies.

The code in this directory should live in namespace ash. While code in
//chrome is not supposed to be in any namespace, //chrome/browser/ash is
technically part of the ash binary. The fact that it lives in //chrome/browser
instead of in //ash is because top level product directories shouldn't be
depended on by any other directory. In the future, when some of the
dependencies from //chrome/browser/ash to //chrome/browser are sorted out,
some of this code will move to //ash.

Most of this code originally came from
[`chrome/browser/chromeos`](/chrome/browser/chromeos/README.md) as part of a
refactoring that split Ash code from the Lacros browser code. See the [Lacros
project](/docs/lacros.md) and the "Lacros: ChromeOS source code directory
migration" design doc at
https://docs.google.com/document/d/1g-98HpzA8XcoGBWUv1gQNr4rbnD5yfvbtYZyPDDbkaE.
