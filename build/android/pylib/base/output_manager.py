# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import contextlib
import logging
import os
import tempfile

from devil.utils import reraiser_thread


class Datatype:
  HTML = 'text/html'
  JSON = 'application/json'
  PNG = 'image/png'
  TEXT = 'text/plain'


class OutputManager:

  def __init__(self):
    """OutputManager Constructor.

    This class provides a simple interface to save test output. Subclasses
    of this will allow users to save test results in the cloud or locally.
    """
    self._allow_upload = False
    self._thread_group = None

  @contextlib.contextmanager
  def ArchivedTempfile(
      self, out_filename, out_subdir, datatype=Datatype.TEXT):
    """Archive file contents asynchonously and then deletes file.

    Args:
      out_filename: Name for saved file.
      out_subdir: Directory to save |out_filename| to.
      datatype: Datatype of file.

    Returns:
      An ArchivedFile file. This file will be uploaded async when the context
      manager exits. AFTER the context manager exits, you can get the link to
      where the file will be stored using the Link() API. You can use typical
      file APIs to write and flish the ArchivedFile. You can also use file.name
      to get the local filepath to where the underlying file exists. If you do
      this, you are responsible of flushing the file before exiting the context
      manager.
    """
    if not self._allow_upload:
      raise Exception('Must run |SetUp| before attempting to upload!')

    f = self.CreateArchivedFile(out_filename, out_subdir, datatype)
    try:
      yield f
    finally:
      self.ArchiveArchivedFile(f, delete=True)

  def CreateArchivedFile(self, out_filename, out_subdir,
                         datatype=Datatype.TEXT):
    """Returns an instance of ArchivedFile."""
    return self._CreateArchivedFile(out_filename, out_subdir, datatype)

  def _CreateArchivedFile(self, out_filename, out_subdir, datatype):
    raise NotImplementedError

  def ArchiveArchivedFile(self, archived_file, delete=False):
    """Archive an ArchivedFile instance and optionally delete it."""
    if not isinstance(archived_file, ArchivedFile):
      raise Exception('Excepting an instance of ArchivedFile, got %s.' %
                      type(archived_file))
    archived_file.PrepareArchive()

    def archive():
      try:
        archived_file.Archive()
      finally:
        if delete:
          archived_file.Delete()

    thread = reraiser_thread.ReraiserThread(func=archive)
    thread.start()
    self._thread_group.Add(thread)

  def SetUp(self):
    self._allow_upload = True
    self._thread_group = reraiser_thread.ReraiserThreadGroup()

  def TearDown(self):
    self._allow_upload = False
    logging.info('Finishing archiving output.')
    self._thread_group.JoinAll()

  def __enter__(self):
    self.SetUp()
    return self

  def __exit__(self, _exc_type, _exc_val, _exc_tb):
    self.TearDown()


class ArchivedFile:

  def __init__(self, out_filename, out_subdir, datatype):
    self._out_filename = out_filename
    self._out_subdir = out_subdir
    self._datatype = datatype

    mode = 'w+'
    if datatype == Datatype.PNG:
      mode = 'w+b'
    self._f = tempfile.NamedTemporaryFile(mode=mode, delete=False)
    self._ready_to_archive = False

  @property
  def name(self):
    return self._f.name

  def fileno(self, *args, **kwargs):
    if self._ready_to_archive:
      raise Exception('Cannot retrieve the integer file descriptor '
                      'after archiving has begun!')
    return self._f.fileno(*args, **kwargs)

  def write(self, *args, **kwargs):
    if self._ready_to_archive:
      raise Exception('Cannot write to file after archiving has begun!')
    self._f.write(*args, **kwargs)

  def flush(self, *args, **kwargs):
    if self._ready_to_archive:
      raise Exception('Cannot flush file after archiving has begun!')
    self._f.flush(*args, **kwargs)

  def Link(self):
    """Returns location of archived file."""
    if not self._ready_to_archive:
      raise Exception('Cannot get link to archived file before archiving '
                      'has begun')
    return self._Link()

  def _Link(self):
    """Note for when overriding this function.

    This function will certainly be called before the file
    has finished being archived. Therefore, this needs to be able to know the
    exact location of the archived file before it is finished being archived.
    """
    raise NotImplementedError

  def PrepareArchive(self):
    """Meant to be called synchronously to prepare file for async archiving."""
    self.flush()
    self._ready_to_archive = True
    self._PrepareArchive()

  def _PrepareArchive(self):
    """Note for when overriding this function.

    This function is needed for things such as computing the location of
    content addressed files. This is called after the file is written but
    before archiving has begun.
    """

  def Archive(self):
    """Archives file."""
    if not self._ready_to_archive:
      raise Exception('File is not ready to archive. Be sure you are not '
                      'writing to the file and PrepareArchive has been called')
    self._Archive()

  def _Archive(self):
    raise NotImplementedError

  def Delete(self):
    """Deletes the backing file."""
    self._f.close()
    os.remove(self.name)
