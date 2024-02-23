// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the valid font names that can be passed to front-end and maps
// them to a corresponding class style in app.html. Must stay in-sync with
// the names set in read_anything_model.cc.
export const defaultFontName: string = 'sans-serif';
const validFontNames: Array<{name: string, css: string}> = [
  {name: 'Poppins', css: 'Poppins'},
  {name: 'Sans-serif', css: 'sans-serif'},
  {name: 'Serif', css: 'serif'},
  {name: 'Comic Neue', css: '"Comic Neue"'},
  {name: 'Lexend Deca', css: '"Lexend Deca"'},
  {name: 'EB Garamond', css: '"EB Garamond"'},
  {name: 'STIX Two Text', css: '"STIX Two Text"'},
  {name: 'Andika', css: 'Andika'},
];

// Validate that the given font name is a valid choice, or use the default.
export function validatedFontName(fontName: string): string {
  const validFontName =
      validFontNames.find((f: {name: string}) => f.name === fontName);
  return validFontName ? validFontName.css : defaultFontName;
}
