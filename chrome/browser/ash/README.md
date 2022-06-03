chrome/browser/ash
==================

This directory should contain non-UI Chrome OS specific code that has
`chrome/browser` dependencies.

The code in this directory should live in namespace ash. While code in
//chrome is not supposed to be in any namespace, //chrome/browser/ash is
technically part of the ash binary. The fact that it lives in //chrome/browser
instead of in //ash is because top level product directories shouldn't be
depended on by any other directory. In the future, when some of the
dependencies from //chrome/browser/ash to //chrome/browser are sorted out,
some of this code will move to //ash.

As of January 2021, code from
[`chrome/browser/chromeos`](/chrome/browser/chromeos/README.md) is migrating
into this directory, as part of the [Lacros project](/docs/lacros.md).

Googlers: See go/lacros-directory-migration for more details.
